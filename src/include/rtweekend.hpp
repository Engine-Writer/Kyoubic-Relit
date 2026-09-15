#ifndef RTWEEKEND_HPP
#define RTWEEKEND_HPP
//==============================================================================================
// To the extent possible under law, the author(s) have dedicated all copyright
// and related and neighboring rights to this software to the public domain
// worldwide. This software is distributed without any warranty.
//
// You should have received a copy (see file COPYING.txt) of the CC0 Public
// Domain Dedication along with this software. If not, see
// <http://creativecommons.org/publicdomain/zero/1.0/>.
//==============================================================================================

#include <cmath>
#include <cstdlib>
#include <limits>
#include <memory>
#include <random>

using std::make_shared;
using std::shared_ptr;
using std::sqrt;

const double infinity = std::numeric_limits<double>::infinity();
const double pi = 3.1415926535897932385;

inline double degrees_to_radians(double degrees) {
    return degrees * pi / 180.0;
}

// Per-thread RNG override hook. Normally null (random_double() uses its own
// ambient thread_local generator, unseeded/arbitrary — fine for pixel
// sampling, where adjacent samples are never expected to correlate). Vertex
// baking installs a position-seeded generator here for the duration of one
// vertex's samples, so two mesh corners sharing the same world position
// (duplicate, unwelded corners at a triangle seam) draw the IDENTICAL sample
// sequence instead of independent noise -- independent noise there is what
// produces hard per-triangle facets after GPU smooth-interpolation, since
// adjacent triangles sharing that seam disagree on what should be a
// continuous value. See scoped_rng_override below.
inline thread_local std::mt19937 *g_rng_override = nullptr;

inline double random_double() {
    static thread_local std::uniform_real_distribution<double> distribution(0.0, 1.0);
    static thread_local std::mt19937 generator;
    if (g_rng_override) return distribution(*g_rng_override);
    return distribution(generator);
}

struct scoped_rng_override {
    std::mt19937 engine;
    std::mt19937 *prev;
    explicit scoped_rng_override(uint64_t seed) : engine(static_cast<uint32_t>(seed)) {
        prev = g_rng_override;
        g_rng_override = &engine;
    }
    ~scoped_rng_override() { g_rng_override = prev; }
};

inline double random_double(double min, double max) {
    return min + (max - min) * random_double();
}

inline int random_int(int min, int max) {
    return static_cast<int>(random_double(min, max + 1));
}

inline double clamp(double x, double min, double max) {
    if (x < min) return min;
    if (x > max) return max;
    return x;
}

#include "interval.hpp"
#include "vec3.hpp"
#include "ray.hpp"

#endif // RTWEEKEND_HPP
