#ifndef CAMERA_HPP
#define CAMERA_HPP
//==============================================================================================
// Originally written in 2016 by Peter Shirley <ptrshrl@gmail.com>
//
// To the extent possible under law, the author(s) have dedicated all copyright and related and
// neighboring rights to this software to the public domain worldwide. This software is
// distributed without any warranty.
//
// You should have received a copy (see file COPYING.txt) of the CC0 Public Domain Dedication
// along with this software. If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.
//==============================================================================================

#include <atomic>
#include <fstream>
#include <thread>
#include <vector>

#include "color.hpp"
#include "hittable.hpp"
#include "material.hpp"
#include "pdf.hpp"
#include "rtweekend.hpp"

class camera {
public:
    double aspect_ratio = 1.0;
    int image_width = 400;
    int samples_per_pixel = 100;
    int max_depth = 10;
    color background;

    double vfov = 90;
    point3 lookfrom = point3(0, 0, 0);
    point3 lookat = point3(0, 0, -1);
    vec3 vup = vec3(0, 1, 0);

    double defocus_angle = 0;
    double focus_dist = 10;

    // The OG code used 1 threads which is slow as balls. now we use MAX THREADS
    int num_threads = std::max(1u, std::thread::hardware_concurrency());

    void render(const hittable &world, const hittable &lights, const std::string &out_path) {
        initialize();

        std::vector<color> framebuffer(image_width * image_height);
        std::atomic<int> next_row{0};

        // I was a bit lazy so I used lambda
        auto worker = [&]() {
            int j;
            while ((j = next_row.fetch_add(1)) < image_height) {
                for (int i = 0; i < image_width; i++) {
                    color pixel_color(0, 0, 0);
                    for (int s_j = 0; s_j < sqrt_spp; s_j++) {
                        for (int s_i = 0; s_i < sqrt_spp; s_i++) {
                            ray r = get_ray(i, j, s_i, s_j);
                            pixel_color += ray_color(r, max_depth, world, lights);
                        }
                    }
                    framebuffer[j * image_width + i] = pixel_samples_scale * pixel_color;
                }
            }
        };

        std::vector<std::thread> pool;
        for (int t = 0; t < num_threads; t++) pool.emplace_back(worker);

        int last_reported = -1;
        while (true) {
            int done = std::min(next_row.load(), image_height);
            if (done != last_reported) {
                std::clog << "\rScanlines remaining: " << (image_height - done) << ' ' << std::flush;
                last_reported = done;
            }
            if (done >= image_height) break;
        }

        for (auto &th : pool) th.join();

        std::ofstream out(out_path, std::ios::binary);
        out << "P3\n" << image_width << ' ' << image_height << "\n255\n";
        for (auto &c : framebuffer) write_color(out, c);

        std::clog << "\nDone.";
    }

private:
    int image_height;
    double pixel_samples_scale;
    int sqrt_spp;
    double recip_sqrt_spp;
    point3 center;
    point3 pixel00_loc;
    vec3 pixel_delta_u;
    vec3 pixel_delta_v;
    vec3 u, v, w;
    vec3 defocus_disk_u;
    vec3 defocus_disk_v;

    void initialize() {
        image_height = int(image_width / aspect_ratio);
        image_height = (image_height < 1) ? 1 : image_height;

        sqrt_spp = int(sqrt(double(samples_per_pixel)));
        pixel_samples_scale = 1.0 / (sqrt_spp * sqrt_spp);
        recip_sqrt_spp = 1.0 / sqrt_spp;

        center = lookfrom;

        auto theta = degrees_to_radians(vfov);
        auto h = tan(theta / 2);
        auto viewport_height = 2 * h * focus_dist;
        auto viewport_width = viewport_height * (double(image_width) / image_height);

        w = unit_vector(lookfrom - lookat);
        u = unit_vector(cross(vup, w));
        v = cross(w, u);

        vec3 viewport_u = viewport_width * u;
        vec3 viewport_v = viewport_height * -v;

        pixel_delta_u = viewport_u / image_width;
        pixel_delta_v = viewport_v / image_height;

        auto viewport_upper_left = center - (focus_dist * w) - viewport_u / 2 - viewport_v / 2;
        pixel00_loc = viewport_upper_left + 0.5 * (pixel_delta_u + pixel_delta_v);

        auto defocus_radius = focus_dist * tan(degrees_to_radians(defocus_angle / 2));
        defocus_disk_u = u * defocus_radius;
        defocus_disk_v = v * defocus_radius;
    }

    ray get_ray(int i, int j, int s_i, int s_j) const {
        auto offset = sample_square_stratified(s_i, s_j);
        auto pixel_sample = pixel00_loc + ((i + offset.x()) * pixel_delta_u);
        pixel_sample += ((j + offset.y()) * pixel_delta_v);

        auto ray_origin = (defocus_angle <= 0) ? center : defocus_disk_sample();
        auto ray_direction = pixel_sample - ray_origin;
        auto ray_time = random_double();

        return ray(ray_origin, ray_direction, ray_time);
    }

    vec3 sample_square_stratified(int s_i, int s_j) const {
        auto px = ((s_i + random_double()) * recip_sqrt_spp) - 0.5;
        auto py = ((s_j + random_double()) * recip_sqrt_spp) - 0.5;
        return vec3(px, py, 0);
    }

    point3 defocus_disk_sample() const {
        auto p = random_in_unit_disk();
        return center + (p[0] * defocus_disk_u) + (p[1] * defocus_disk_v);
    }

    color ray_color(const ray &r, int depth, const hittable &world, const hittable &lights) const {
        if (depth <= 0) return color(0, 0, 0);

        hit_record rec;

        if (!world.hit(r, interval(0.001, infinity), rec)) return background;

        scatter_record srec;
        color color_from_emission = rec.mat->emitted(r, rec, rec.u, rec.v, rec.p);

        if (!rec.mat->scatter(r, rec, srec)) return color_from_emission;

        if (srec.skip_pdf) {
            return srec.attenuation * ray_color(srec.skip_pdf_ray, depth - 1, world, lights);
        }

        auto light_ptr = make_shared<hittable_pdf>(lights, rec.p);
        mixture_pdf p(light_ptr, srec.pdf_ptr);

        ray scattered = ray(rec.p, p.generate(), r.time());
        auto pdf_val = p.value(scattered.direction());
        if (pdf_val <= 0) return color_from_emission;

        double scattering_pdf = rec.mat->scattering_pdf(r, rec, scattered);

        color sample_color = ray_color(scattered, depth - 1, world, lights);
        color color_from_scatter = (srec.attenuation * scattering_pdf * sample_color) / pdf_val;

        return color_from_emission + color_from_scatter;
    }
};

#endif // CAMERA_HPP
