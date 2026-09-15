#include "include/rtweekend.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <ostream>
#include <sstream>
#include <thread>
#include <vector>

#include "include/bvh.hpp"
#include "include/hittable.hpp"
#include "include/hittable_list.hpp"
#include "include/material.hpp"
#include "include/obj.hpp"
#include "include/onb.hpp"
#include "include/pdf.hpp"
#include "include/quad.hpp"
#include "include/sphere.hpp"
#include "include/triangle.hpp"


class WeightedLightList : public hittable {
public:
    void add(shared_ptr<hittable> object, double power) {
        objects.push_back(object);
        double w = std::max(power, 1e-6);
        weights.push_back(w);
        totalWeight += w;
        bbox = aabb(bbox, object->bounding_box());
    }

    bool hit(const ray &r, interval ray_t, hit_record &rec) const override {
        hit_record tempRec;
        bool hitAnything = false;
        auto closestSoFar = ray_t.max;
        for (const auto &object : objects) {
            if (object->hit(r, interval(ray_t.min, closestSoFar), tempRec)) {
                hitAnything = true;
                closestSoFar = tempRec.t;
                rec = tempRec;
            }
        }
        return hitAnything;
    }

    aabb bounding_box() const override { return bbox; }

    double pdf_value(const point3 &origin, const vec3 &direction) const override {
        double sum = 0.0;
        for (size_t i = 0; i < objects.size(); i++) {
            double p = weights[i] / totalWeight;
            sum += p * objects[i]->pdf_value(origin, direction);
        }
        return sum;
    }

    vec3 random(const point3 &origin) const override {
        double r = random_double() * totalWeight;
        for (size_t i = 0; i < objects.size(); i++) {
            if (r < weights[i]) return objects[i]->random(origin);
            r -= weights[i];
        }
        return objects.back()->random(origin);
    }

private:
    std::vector<shared_ptr<hittable>> objects;
    std::vector<double> weights;
    double totalWeight = 0.0;
    aabb bbox;
};

static color bakeRayColor(
    const ray &r, int depth, const hittable &world, const hittable &lights
) {
    if (depth <= 0) return color(0, 0, 0);

    hit_record rec;
    if (!world.hit(r, interval(0.001, infinity), rec)) return color(0, 0, 0);

    scatter_record srec;
    color emitted = rec.mat->emitted(r, rec, rec.u, rec.v, rec.p);

    if (!rec.mat->scatter(r, rec, srec)) return emitted;

    if (srec.skip_pdf)
        return srec.attenuation * bakeRayColor(srec.skip_pdf_ray, depth - 1, world, lights);

    auto lightPtr = make_shared<hittable_pdf>(lights, rec.p);
    mixture_pdf p(lightPtr, srec.pdf_ptr);

    ray scattered(rec.p, p.generate(), r.time());
    auto pdfVal = p.value(scattered.direction());
    if (pdfVal <= 0) return emitted;

    double scatteringPdf = rec.mat->scattering_pdf(r, rec, scattered);
    color sample = bakeRayColor(scattered, depth - 1, world, lights);
    color fromScatter = (srec.attenuation * scatteringPdf * sample) / pdfVal;

    return emitted + fromScatter;
}

// unweld duplicate corners to unshitify geometry
static uint64_t hashWorldPos(const point3 &p, const vec3 &n) {
    auto mix = [](uint64_t h, int64_t v) {
        h ^= static_cast<uint64_t>(v) + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
        return h;
    };
    auto quantize = [](double v) { return static_cast<int64_t>(std::lround(v * 1000.0)); };

    uint64_t h = 1469598103934665603ull;

    h = mix(h, quantize(p.x()));
    h = mix(h, quantize(p.y()));
    h = mix(h, quantize(p.z()));
    h = mix(h, quantize(n.x()));
    h = mix(h, quantize(n.y()));
    h = mix(h, quantize(n.z()));
    return h;
}

static vec3 stratifiedCosineDirection(int sampleI, int sampleJ, int gridN) {
    auto r1 = (sampleI + random_double()) / gridN;
    auto r2 = (sampleJ + random_double()) / gridN;

    auto phi = 2 * pi * r1;
    auto x = cos(phi) * sqrt(r2);
    auto y = sin(phi) * sqrt(r2);

    auto z = sqrt(1 - r2);
    return vec3(x, y, z);
}

static color bakeVertex(
    const point3 &pos, const vec3 &normal, const hittable &world,
    const hittable &lights, int spp, int maxDepth, double fireflyClamp
) {
    scoped_rng_override rngScope(hashWorldPos(pos, normal));

    onb uvw(normal);
    color sum(0, 0, 0);
    point3 origin = pos + 0.001 * normal;

    auto lightPtr = make_shared<hittable_pdf>(lights, origin);
    auto cosPtr = make_shared<cosine_pdf>(normal);

    auto misWeightAndPdf = [&](const vec3 &dir) {
        return 0.5 * lightPtr->value(dir) + 0.5 * cosPtr->value(dir);
    };

    auto accumulate = [&](const vec3 &dir) {
        double pdfVal = misWeightAndPdf(dir);
        if (pdfVal <= 0) return;

        double cosTheta = dot(normal, unit_vector(dir));
        if (cosTheta <= 0) return;

        ray r(origin, dir, 0.0);
        color incoming = bakeRayColor(r, maxDepth, world, lights);
        color contribution = (incoming * cosTheta) / pdfVal;

        double lum =
            0.2126 * contribution.x() + 0.7152 * contribution.y() + 0.0722 * contribution.z();
        if (lum > fireflyClamp) contribution *= (fireflyClamp / lum);

        sum += contribution;
    };

    int half = spp / 2;
    int gridN = std::max(1, static_cast<int>(std::sqrt(static_cast<double>(half))));

    for (int s = 0; s < half; s++) accumulate(lightPtr->generate());

    for (int sJ = 0; sJ < gridN; sJ++) {
        for (int sI = 0; sI < gridN; sI++) {
            vec3 localDir = stratifiedCosineDirection(sI, sJ, gridN);
            accumulate(uvw.transform(localDir));
        }
    }
    int cosineCount = gridN * gridN;

    for (int s = half + cosineCount; s < spp; s++) accumulate(uvw.transform(random_cosine_direction()));

    int total = half + cosineCount + std::max(0, spp - half - cosineCount);
    return sum / double(total);
}

struct BakeTarget {
    std::string objPath;
    std::string key;
    int spp;
};

struct EmissiveBox {
    point3 lo, hi;
    color emission;
};

static std::vector<EmissiveBox> parseLights(const std::string &spec) {
    std::vector<EmissiveBox> out;
    if (spec.empty()) return out;

    std::istringstream ss(spec);
    std::string tok;
    while (std::getline(ss, tok, ';')) {
        std::istringstream fs(tok);
        std::string field;
        std::vector<double> vals;

        while (std::getline(fs, field, ':')) vals.push_back(std::stod(field));
        if (vals.size() != 9) continue;

        EmissiveBox b;
        b.lo = point3(vals[0], vals[1], vals[2]);
        b.hi = point3(vals[3], vals[4], vals[5]);
        b.emission = color(vals[6], vals[7], vals[8]);
        out.push_back(b);
    }
    return out;
}

static void addBoxQuads(hittable_list &target, const point3 &lo, const point3 &hi, shared_ptr<material> mat) {
    vec3 dx(hi.x() - lo.x(), 0, 0);
    vec3 dy(0, hi.y() - lo.y(), 0);
    vec3 dz(0, 0, hi.z() - lo.z());

    target.add(make_shared<quad>(point3(lo.x(), lo.y(), hi.z()), dx, dy, mat));
    target.add(make_shared<quad>(point3(hi.x(), lo.y(), hi.z()), -dz, dy, mat));
    target.add(make_shared<quad>(point3(hi.x(), lo.y(), lo.z()), -dx, dy, mat));
    target.add(make_shared<quad>(point3(lo.x(), lo.y(), lo.z()), dz, dy, mat));
    target.add(make_shared<quad>(point3(lo.x(), hi.y(), hi.z()), dx, -dz, mat));
    target.add(make_shared<quad>(point3(lo.x(), lo.y(), lo.z()), dx, dz, mat));
}

static double boxLightPower(const point3 &lo, const point3 &hi, const color &emission) {
    double dx = hi.x() - lo.x(), dy = hi.y() - lo.y(), dz = hi.z() - lo.z();
    double surfaceArea = 2.0 * (dx * dy + dy * dz + dz * dx);
    double radiance = emission.x() + emission.y() + emission.z();
    return surfaceArea * radiance;
}

static bool addSceneGeometry(hittable_list &world, const std::string &objPathsCsv) {
    auto grey = make_shared<lambertian>(color(0.5, 0.5, 0.5));

    std::vector<std::string> objPaths;
    std::istringstream ss(objPathsCsv);
    std::string tok;
    while (std::getline(ss, tok, ',')) objPaths.push_back(tok);

    for (auto &path : objPaths) {
        obj_mesh sceneMesh;
        if (!load_obj_corners(path, sceneMesh)) {
            std::cerr << "Failed to load scene obj: " << path << std::endl;
            return false;
        }
        for (auto &tri : sceneMesh.faces) {
            world.add(make_shared<triangle>(
                tri[0].pos, tri[1].pos, tri[2].pos,
                tri[0].normal, tri[1].normal, tri[2].normal, grey
            ));
        }
    }
    return true;
}

static double addLightBoxes(
    hittable_list &world, WeightedLightList &weightedLights,
    const std::string &lightsSpec
) {
    double maxLightChannel = 0.0;
    for (auto &box : parseLights(lightsSpec)) {
        auto emit = make_shared<diffuse_light>(box.emission);
        addBoxQuads(world, box.lo, box.hi, emit);

        auto boxQuads = make_shared<hittable_list>();
        addBoxQuads(*boxQuads, box.lo, box.hi, emit);
        weightedLights.add(boxQuads, boxLightPower(box.lo, box.hi, box.emission));

        maxLightChannel = std::max({
            maxLightChannel, box.emission.x(), box.emission.y(), box.emission.z()
        });
    }
    return std::max(maxLightChannel, 1.0) * 8.0;
}

struct CornerRef {
    point3 pos;
    vec3 normal;
};

static std::vector<CornerRef> flattenCorners(const obj_mesh &mesh) {
    std::vector<CornerRef> corners;
    for (auto &tri : mesh.faces)
        for (auto &c : tri) corners.push_back({ c.pos, c.normal });

    return corners;
}

static std::vector<color> bakeTargetCorners(
    const std::vector<CornerRef> &corners, const hittable &worldBvh, const hittable &weightedLights,
    const BakeTarget &target, int maxDepth, double fireflyClamp
) {
    std::vector<color> results(corners.size());
    std::atomic<size_t> next{ 0 };
    std::atomic<size_t> done{ 0 };

    int numThreads = std::max(1u, std::thread::hardware_concurrency());
    auto worker = [&]() {
        size_t i;
        while ((i = next.fetch_add(1)) < corners.size()) {
            results[i] = bakeVertex(
                corners[i].pos, corners[i].normal, worldBvh, weightedLights, target.spp, maxDepth, fireflyClamp
            );
            done.fetch_add(1);
        }
    };

    std::vector<std::thread> pool;
    for (int t = 0; t < numThreads; t++) pool.emplace_back(worker);

    size_t lastReported = SIZE_MAX;
    while (true) {
        size_t d = std::min(done.load(), corners.size());
        if (d != lastReported) {
            std::cout << "\r  " << d << "/" << corners.size() << std::flush;
            lastReported = d;
        }
        if (d >= corners.size()) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
    for (auto &th : pool) th.join();
    std::cout << std::endl;

    return results;
}

static std::vector<BakeTarget> parseBakeTargets(int argc, char** argv) {
    std::vector<BakeTarget> targets;
    for (int i = 4; i + 2 < argc; i += 3) {
        BakeTarget t;
        t.objPath = argv[i];
        t.key = argv[i + 1];
        t.spp = std::atoi(argv[i + 2]);
        targets.push_back(t);
    }
    return targets;
}

int main(int argc, char** argv) {
    if (argc < 7 || (argc - 4) % 3 != 0) {
        std::cerr << "Usage: " << argv[0];
        std::cerr << " <scene_objs_comma_separated> <light_boxes_semicolon_separated_or_empty>";
        std::cerr << " <out.txt> <bake.obj> <key> <spp> [...]" << std::endl;
        std::cerr << "  light box spec per light: x0:y0:z0:x1:y1:z1:r:g:b" << std::endl;
        return 1;
    }

    std::string sceneObjsCsv = argv[1];
    std::string lightsSpec = argv[2];
    std::string outPath = argv[3];
    std::vector<BakeTarget> targets = parseBakeTargets(argc, argv);

    hittable_list world;
    if (!addSceneGeometry(world, sceneObjsCsv)) return 1;

    WeightedLightList weightedLights;
    double fireflyClamp = addLightBoxes(world, weightedLights, lightsSpec);

    bvh_node worldBvh(world);
    int maxDepth = 8;

    std::ofstream out(outPath);
    out << "# per-vertex lightbake (raytracer_weekend), format: key corner_count then r g b per corner\n";

    for (auto &target : targets) {
        obj_mesh mesh;
        if (!load_obj_corners(target.objPath, mesh)) {
            std::cerr << "Failed to load bake target obj: " << target.objPath;
            std::cerr << std::endl;
            continue;
        }

        std::vector<CornerRef> corners = flattenCorners(mesh);
        std::cout << "Baking '" << target.key << "': " << corners.size();
        std::cout << " corners @ " << target.spp << " spp" << std::endl;

        std::vector<color> results = bakeTargetCorners(
            corners, worldBvh, weightedLights,
            target, maxDepth, fireflyClamp
        );

        out << target.key << " " << corners.size() << "\n";
        for (auto &c : results) out << c.x() << " " << c.y() << " " << c.z() << "\n";
    }

    out.close();
    std::cout << "Wrote " << outPath << std::endl;
    return 0;
}
