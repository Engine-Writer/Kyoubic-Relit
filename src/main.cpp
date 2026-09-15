#include "include/rtweekend.hpp"

#include "include/bvh.hpp"
#include "include/camera.hpp"
#include "include/constant_medium.hpp"
#include "include/hittable.hpp"
#include "include/hittable_list.hpp"
#include "include/material.hpp"
#include "include/obj.hpp"
#include "include/quad.hpp"
#include "include/sphere.hpp"
#include "include/texture.hpp"
#include "include/triangle.hpp"

#include <iostream>

void cornell_box() {
    hittable_list world;

    auto red = make_shared<lambertian>(color(.65, .05, .05));
    auto white = make_shared<lambertian>(color(.73, .73, .73));
    auto green = make_shared<lambertian>(color(.12, .45, .15));
    auto light = make_shared<diffuse_light>(color(15, 15, 15));

    world.add(make_shared<quad>(point3(555, 0, 0), vec3(0, 555, 0), vec3(0, 0, 555), green));
    world.add(make_shared<quad>(point3(0, 0, 0), vec3(0, 555, 0), vec3(0, 0, 555), red));
    world.add(make_shared<quad>(point3(343, 554, 332), vec3(-130, 0, 0), vec3(0, 0, -105), light));
    world.add(make_shared<quad>(point3(0, 0, 0), vec3(555, 0, 0), vec3(0, 0, 555), white));
    world.add(make_shared<quad>(point3(555, 555, 555), vec3(-555, 0, 0), vec3(0, 0, -555), white));
    world.add(make_shared<quad>(point3(0, 0, 555), vec3(555, 0, 0), vec3(0, 555, 0), white));

    shared_ptr<hittable> box1 = box(point3(0, 0, 0), point3(165, 330, 165), white);
    box1 = make_shared<rotate_y>(box1, 15);
    box1 = make_shared<translate>(box1, vec3(265, 0, 295));
    world.add(box1);

    auto glass = make_shared<dielectric>(1.5);
    world.add(make_shared<sphere>(point3(190, 90, 190), 90, glass));

    hittable_list lights;
    auto empty_material = shared_ptr<material>();
    lights.add(make_shared<quad>(
        point3(343, 554, 332), vec3(-130, 0, 0), vec3(0, 0, -105),
        empty_material)
    );
    lights.add(make_shared<sphere>(point3(190, 90, 190), 90, empty_material));

    bvh_node world_bvh(world);
    hittable_list bvh_world;
    bvh_world.add(make_shared<bvh_node>(world_bvh));

    camera cam;

    cam.aspect_ratio = 1.0;
    cam.image_width = 600;
    cam.samples_per_pixel = 256;
    cam.max_depth = 50;
    cam.background = color(0, 0, 0);

    cam.vfov = 40;
    cam.lookfrom = point3(278, 278, -800);
    cam.lookat = point3(278, 278, 0);
    cam.vup = vec3(0, 1, 0);

    cam.defocus_angle = 0;

    cam.render(world_bvh, lights, "cornell_box.ppm");
}

void final_scene(int image_width, int samples_per_pixel, int max_depth) {
    hittable_list boxes1;
    auto ground = make_shared<lambertian>(color(0.48, 0.83, 0.53));

    int boxes_per_side = 20;
    for (int i = 0; i < boxes_per_side; i++) {
        for (int j = 0; j < boxes_per_side; j++) {
            auto w = 100.0;
            auto x0 = -1000.0 + i * w;
            auto z0 = -1000.0 + j * w;
            auto y0 = 0.0;
            auto x1 = x0 + w;
            auto y1 = random_double(1, 101);
            auto z1 = z0 + w;

            boxes1.add(box(point3(x0, y0, z0), point3(x1, y1, z1), ground));
        }
    }

    hittable_list world;
    world.add(make_shared<bvh_node>(boxes1));

    auto light = make_shared<diffuse_light>(color(7, 7, 7));
    world.add(make_shared<quad>(point3(123, 554, 147), vec3(300, 0, 0), vec3(0, 0, 265), light));

    auto center1 = point3(400, 400, 200);
    auto center2 = center1 + vec3(30, 0, 0);
    auto sphere_material = make_shared<lambertian>(color(0.7, 0.3, 0.1));
    world.add(make_shared<sphere>(center1, center2, 50, sphere_material));

    world.add(make_shared<sphere>(point3(260, 150, 45), 50, make_shared<dielectric>(1.5)));
    world.add(make_shared<sphere>(point3(0, 150, 145), 50, make_shared<metal>(color(0.8, 0.8, 0.9), 1.0)));

    auto boundary = make_shared<sphere>(point3(360, 150, 145), 70, make_shared<dielectric>(1.5));
    world.add(boundary);
    world.add(make_shared<constant_medium>(boundary, 0.2, color(0.2, 0.4, 0.9)));
    boundary = make_shared<sphere>(point3(0, 0, 0), 5000, make_shared<dielectric>(1.5));
    world.add(make_shared<constant_medium>(boundary, .0001, color(1, 1, 1)));

    auto emat = make_shared<lambertian>(make_shared<noise_texture>(0.1));
    world.add(make_shared<sphere>(point3(220, 280, 300), 80, emat));

    hittable_list boxes2;
    auto white = make_shared<lambertian>(color(.73, .73, .73));
    int ns = 1000;
    for (int j = 0; j < ns; j++) {
        boxes2.add(make_shared<sphere>(point3::random(0, 165), 10, white));
    }

    world.add(make_shared<translate>(
        make_shared<rotate_y>(make_shared<bvh_node>(boxes2), 15), vec3(-100, 270, 395)));

    bvh_node world_bvh(world);

    hittable_list lights;
    auto empty_material = shared_ptr<material>();
    lights.add(make_shared<quad>(point3(123, 554, 147), vec3(300, 0, 0), vec3(0, 0, 265), empty_material));

    camera cam;

    cam.aspect_ratio = 1.0;
    cam.image_width = image_width;
    cam.samples_per_pixel = samples_per_pixel;
    cam.max_depth = max_depth;
    cam.background = color(0, 0, 0);

    cam.vfov = 40;
    cam.lookfrom = point3(478, 278, -600);
    cam.lookat = point3(278, 278, 0);
    cam.vup = vec3(0, 1, 0);

    cam.defocus_angle = 0;

    cam.render(world_bvh, lights, "final_scene.ppm");
}


static void add_box_light(
    hittable_list &world, hittable_list &lights, const point3 &lo,
    const point3 &hi, const color &emission
) {
    auto mat = make_shared<diffuse_light>(emission);
    vec3 dx(hi.x() - lo.x(), 0, 0);
    vec3 dy(0, hi.y() - lo.y(), 0);
    vec3 dz(0, 0, hi.z() - lo.z());

    auto add = [&](shared_ptr<quad> q) {
        world.add(q);
        lights.add(q);
    };
    add(make_shared<quad>(point3(lo.x(), lo.y(), hi.z()), dx, dy, mat));
    add(make_shared<quad>(point3(hi.x(), lo.y(), hi.z()), -dz, dy, mat));
    add(make_shared<quad>(point3(hi.x(), lo.y(), lo.z()), -dx, dy, mat));
    add(make_shared<quad>(point3(lo.x(), lo.y(), lo.z()), dz, dy, mat));
    add(make_shared<quad>(point3(lo.x(), hi.y(), hi.z()), dx, -dz, mat));
    add(make_shared<quad>(point3(lo.x(), lo.y(), lo.z()), dx, dz, mat));
}


// Debug sht
void vulkanproject_scene() {
    hittable_list world;
    hittable_list lights;

    auto grey = make_shared<lambertian>(color(0.5, 0.5, 0.5));
    auto green_plastic = make_shared<lambertian>(color(0.1, 0.8, 0.1));

    const char *occluder_paths[2] = {
        "../VulkanProject/scratch_rtweekend_bake/occluder_0.obj",
        "../VulkanProject/scratch_rtweekend_bake/occluder_4.obj",
    };

    const shared_ptr<material> occluder_mats[2] = {grey, green_plastic};
    for (int i = 0; i < 2; i++) {
        obj_mesh mesh;
        if (!load_obj_corners(occluder_paths[i], mesh)) {
            std::cerr << "Failed to load " << occluder_paths[i] << "\n";
            continue;
        }
        for (auto &tri : mesh.faces) {
            world.add(make_shared<triangle>(
                tri[0].pos, tri[1].pos, tri[2].pos, tri[0].normal,
                tri[1].normal, tri[2].normal, occluder_mats[i])
            );
        }
    }

    
    add_box_light(
        world, lights, point3(-2.5, -1.68, 2.43), point3(2.5, 3.32, 7.43),
        color(7.975, 10.875, 14.5)
    );
    add_box_light(
        world, lights, point3(-8.0, -9.7, 0.0), point3(-5.0, -6.7, 3.0),
        color(10.5, 8.4, 2.1)
    );
    add_box_light(
        world, lights, point3(5.2, -9.5, 0.2), point3(7.8, -6.9, 2.8),
        color(2.1, 5.6, 10.5)
    );

    bvh_node world_bvh(world);

    camera cam;
    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = 500;
    cam.samples_per_pixel = 128;
    cam.max_depth = 12;
    cam.background = color(0, 0, 0);

    cam.vfov = 60;
    cam.lookfrom = point3(0, -6, -6);
    cam.lookat = point3(0, -8, 1.5);
    cam.vup = vec3(0, 1, 0);
    cam.defocus_angle = 0;

    // idk use image magick to convert to PNG ig
    // https://imagemagick.org

    cam.render(world_bvh, lights, "vulkanproject_scene.ppm");
}

int main(int argc, char** argv) {
    int scene = argc > 1 ? std::atoi(argv[1]) : 1;

    switch (scene) {
        case 1: cornell_box(); break;
        case 2: final_scene(400, 250, 4); break;
        case 3: vulkanproject_scene(); break;
        default: cornell_box(); break;
    }
}
