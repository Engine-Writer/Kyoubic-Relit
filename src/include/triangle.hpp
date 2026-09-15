#ifndef TRIANGLE_HPP
#define TRIANGLE_HPP

#include "hittable.hpp"
#include "rtweekend.hpp"

class triangle : public hittable {
public:
    triangle(const point3 &v0, const point3 &v1, const point3 &v2, shared_ptr<material> mat)
        : v0(v0), v1(v1), v2(v2), mat(mat) {
        initFaceNormal();
        set_bounding_box();
    }

    triangle(
        const point3 &v0, const point3 &v1, const point3 &v2, const vec3 &n0,
        const vec3 &n1, const vec3 &n2, shared_ptr<material> mat
    ) : v0(v0), v1(v1), v2(v2), n0(n0), n1(n1), n2(n2), has_vertex_normals(true), mat(mat) {
        initFaceNormal();
        set_bounding_box();
    }

    void set_bounding_box() {
        aabb box1(v0, v1);
        aabb box2(v0, v2);
        bbox = aabb(box1, box2);
    }

    void initFaceNormal() {
        vec3 n = cross(v1 - v0, v2 - v0);
        area = n.length() * 0.5;
        face_normal = (area > 1e-12) ? unit_vector(n) : vec3(0, 1, 0);
    }

    aabb bounding_box() const override { return bbox; }

    bool hit(const ray &r, interval ray_t, hit_record &rec) const override {
        // Moller-Trumbore
        vec3 edge1 = v1 - v0;
        vec3 edge2 = v2 - v0;
        vec3 pvec = cross(r.direction(), edge2);
        double det = dot(edge1, pvec);

        if (fabs(det) < 1e-10) return false;
        double inv_det = 1.0 / det;

        vec3 tvec = r.origin() - v0;
        double u = dot(tvec, pvec) * inv_det;
        if (u < 0 || u > 1) return false;

        vec3 qvec = cross(tvec, edge1);
        double v = dot(r.direction(), qvec) * inv_det;
        if (v < 0 || u + v > 1) return false;

        double t = dot(edge2, qvec) * inv_det;
        if (!ray_t.surrounds(t)) return false;

        rec.t = t;
        rec.p = r.at(t);
        rec.u = u;
        rec.v = v;
        rec.mat = mat;

        vec3 outward_normal = face_normal;
        if (has_vertex_normals)
            outward_normal = unit_vector((1 - u - v) * n0 + u * n1 + v * n2);
        
        rec.set_face_normal(r, outward_normal);

        return true;
    }

    double pdf_value(const point3 &origin, const vec3 &direction) const override {
        hit_record rec;
        if (!this->hit(ray(origin, direction), interval(0.001, infinity), rec)) return 0;

        auto distance_squared = rec.t * rec.t * direction.length_squared();
        auto cosine = fabs(dot(direction, rec.normal) / direction.length());
        if (cosine < 1e-8) return 0;

        return distance_squared / (cosine * area);
    }

    vec3 random(const point3 &origin) const override {
        auto r1 = random_double();
        auto r2 = random_double();
        auto su0 = sqrt(r1);
        auto b0 = 1 - su0;
        auto b1 = r2 * su0;
        point3 p = b0 * v0 + b1 * v1 + (1 - b0 - b1) * v2;
        return p - origin;
    }

private:
    point3 v0, v1, v2;
    vec3 n0, n1, n2;
    bool has_vertex_normals = false;
    vec3 face_normal;
    double area;
    shared_ptr<material> mat;
    aabb bbox;
};

#endif // TRIANGLE_HPP
