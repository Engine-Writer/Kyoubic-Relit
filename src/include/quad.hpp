#ifndef QUAD_HPP
#define QUAD_HPP
//==============================================================================================
// To the extent possible under law, the author(s) have dedicated all copyright and related and
// neighboring rights to this software to the public domain worldwide. This software is
// distributed without any warranty.
//
// You should have received a copy (see file COPYING.txt) of the CC0 Public Domain Dedication
// along with this software. If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.
//==============================================================================================

#include "hittable.hpp"
#include "hittable_list.hpp"
#include "rtweekend.hpp"

class quad : public hittable {
public:
    quad(const point3 &Q, const vec3 &u, const vec3 &v, shared_ptr<material> mat)
        : Q(Q), u(u), v(v), mat(mat) {
        auto n = cross(u, v);
        normal = unit_vector(n);
        D = dot(normal, Q);
        w = n / dot(n, n);
        area = n.length();

        set_bounding_box();
    }

    virtual void set_bounding_box() {
        auto bbox_diagonal1 = aabb(Q, Q + u + v);
        auto bbox_diagonal2 = aabb(Q + u, Q + v);
        bbox = aabb(bbox_diagonal1, bbox_diagonal2);
    }

    aabb bounding_box() const override { return bbox; }

    bool hit(const ray &r, interval ray_t, hit_record &rec) const override {
        auto denom = dot(normal, r.direction());

        if (fabs(denom) < 1e-8) return false;

        auto t = (D - dot(normal, r.origin())) / denom;
        if (!ray_t.contains(t)) return false;

        auto intersection = r.at(t);
        vec3 planar_hitpt_vector = intersection - Q;
        auto alpha = dot(w, cross(planar_hitpt_vector, v));
        auto beta = dot(w, cross(u, planar_hitpt_vector));

        if (!is_interior(alpha, beta, rec)) return false;

        rec.t = t;
        rec.p = intersection;
        rec.mat = mat;
        rec.set_face_normal(r, normal);

        return true;
    }

    virtual bool is_interior(double a, double b, hit_record &rec) const {
        interval unit_interval(0, 1);
        if (!unit_interval.contains(a) || !unit_interval.contains(b)) return false;

        rec.u = a;
        rec.v = b;
        return true;
    }

    double pdf_value(const point3 &origin, const vec3 &direction) const override {
        hit_record rec;
        if (!this->hit(ray(origin, direction), interval(0.001, infinity), rec)) return 0;

        // Clamp near-field distance for a bake-vertex sitting almost flush
        // against a small emitter (e.g. a light-box wall a few centimeters
        // from the floor it illuminates): true point-sampled irradiance from
        // a finite-area light diverges as distance->0, which is correct for
        // a pinhole camera ray but produces unusable near-white singularities
        // when baking irradiance directly AT surface vertices next to the
        // light. A minimum-distance floor (sqrt of light area) bounds this
        // the way a small but non-zero solid angle naturally would.
        auto min_dist = sqrt(area);
        auto distance_squared = fmax(rec.t * rec.t * direction.length_squared(), min_dist * min_dist);
        auto cosine = fabs(dot(direction, rec.normal) / direction.length());

        return distance_squared / (cosine * area);
    }

    vec3 random(const point3 &origin) const override {
        auto p = Q + (random_double() * u) + (random_double() * v);
        return p - origin;
    }

private:
    point3 Q;
    vec3 u, v;
    vec3 w;
    shared_ptr<material> mat;
    aabb bbox;
    vec3 normal;
    double D;
    double area;
};

inline shared_ptr<hittable_list> box(const point3 &a, const point3 &b, shared_ptr<material> mat) {
    auto sides = make_shared<hittable_list>();

    auto min = point3(fmin(a.x(), b.x()), fmin(a.y(), b.y()), fmin(a.z(), b.z()));
    auto max = point3(fmax(a.x(), b.x()), fmax(a.y(), b.y()), fmax(a.z(), b.z()));

    auto dx = vec3(max.x() - min.x(), 0, 0);
    auto dy = vec3(0, max.y() - min.y(), 0);
    auto dz = vec3(0, 0, max.z() - min.z());

    sides->add(make_shared<quad>(point3(min.x(), min.y(), max.z()), dx, dy, mat));
    sides->add(make_shared<quad>(point3(max.x(), min.y(), max.z()), -dz, dy, mat));
    sides->add(make_shared<quad>(point3(max.x(), min.y(), min.z()), -dx, dy, mat));
    sides->add(make_shared<quad>(point3(min.x(), min.y(), min.z()), dz, dy, mat));
    sides->add(make_shared<quad>(point3(min.x(), max.y(), max.z()), dx, -dz, mat));
    sides->add(make_shared<quad>(point3(min.x(), min.y(), min.z()), dx, dz, mat));

    return sides;
}

#endif // QUAD_HPP
