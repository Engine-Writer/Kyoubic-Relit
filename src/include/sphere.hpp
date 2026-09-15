#ifndef SPHERE_HPP
#define SPHERE_HPP
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

#include "hittable.hpp"
#include "onb.hpp"
#include "rtweekend.hpp"

class sphere : public hittable {
public:
    // Stationary sphere
    sphere(const point3 &static_center, double radius, shared_ptr<material> mat)
        : center(static_center, vec3(0, 0, 0)), radius(fmax(0, radius)), mat(mat) {
        auto rvec = vec3(radius, radius, radius);
        bbox = aabb(static_center - rvec, static_center + rvec);
    }

    // Moving sphere
    sphere(const point3 &center1, const point3 &center2, double radius, shared_ptr<material> mat)
        : center(center1, center2 - center1), radius(fmax(0, radius)), mat(mat) {
        auto rvec = vec3(radius, radius, radius);
        aabb box1(center.at(0) - rvec, center.at(0) + rvec);
        aabb box2(center.at(1) - rvec, center.at(1) + rvec);
        bbox = aabb(box1, box2);
    }

    bool hit(const ray &r, interval ray_t, hit_record &rec) const override {
        point3 current_center = center.at(r.time());
        vec3 oc = current_center - r.origin();
        auto a = r.direction().length_squared();
        auto h = dot(r.direction(), oc);
        auto c = oc.length_squared() - radius * radius;

        auto discriminant = h * h - a * c;
        if (discriminant < 0) return false;

        auto sqrtd = sqrt(discriminant);

        auto root = (h - sqrtd) / a;
        if (!ray_t.surrounds(root)) {
            root = (h + sqrtd) / a;
            if (!ray_t.surrounds(root)) return false;
        }

        rec.t = root;
        rec.p = r.at(rec.t);
        vec3 outward_normal = (rec.p - current_center) / radius;
        rec.set_face_normal(r, outward_normal);
        get_sphere_uv(outward_normal, rec.u, rec.v);
        rec.mat = mat;

        return true;
    }

    aabb bounding_box() const override { return bbox; }

    double pdf_value(const point3 &origin, const vec3 &direction) const override {
        hit_record rec;
        if (!this->hit(ray(origin, direction), interval(0.001, infinity), rec)) return 0;

        auto current_center = center.at(0);
        auto dist_squared = (current_center - origin).length_squared();
        auto cos_theta_max = sqrt(1 - radius * radius / dist_squared);
        auto solid_angle = 2 * pi * (1 - cos_theta_max);

        return 1 / solid_angle;
    }

    vec3 random(const point3 &origin) const override {
        auto current_center = center.at(0);
        vec3 direction = current_center - origin;
        auto distance_squared = direction.length_squared();
        onb uvw(direction);
        return uvw.transform(random_to_sphere(radius, distance_squared));
    }

private:
    ray center;
    double radius;
    shared_ptr<material> mat;
    aabb bbox;

    static void get_sphere_uv(const point3 &p, double &u, double &v) {
        auto theta = acos(-p.y());
        auto phi = atan2(-p.z(), p.x()) + pi;

        u = phi / (2 * pi);
        v = theta / pi;
    }

    static vec3 random_to_sphere(double radius, double distance_squared) {
        auto r1 = random_double();
        auto r2 = random_double();
        auto z = 1 + r2 * (sqrt(1 - radius * radius / distance_squared) - 1);

        auto phi = 2 * pi * r1;
        auto x = cos(phi) * sqrt(1 - z * z);
        auto y = sin(phi) * sqrt(1 - z * z);

        return vec3(x, y, z);
    }
};

#endif // SPHERE_HPP
