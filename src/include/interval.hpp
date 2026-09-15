#ifndef INTERVAL_HPP
#define INTERVAL_HPP
//==============================================================================================
// To the extent possible under law, the author(s) have dedicated all copyright and related and
// neighboring rights to this software to the public domain worldwide. This software is
// distributed without any warranty.
//
// You should have received a copy (see file COPYING.txt) of the CC0 Public Domain Dedication
// along with this software. If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.
//==============================================================================================

class interval {
public:
    double min, max;

    interval() : min(+infinity_iv()), max(-infinity_iv()) {}
    interval(double min, double max) : min(min), max(max) {}

    interval(const interval &a, const interval &b) {
        min = a.min <= b.min ? a.min : b.min;
        max = a.max >= b.max ? a.max : b.max;
    }

    double size() const { return max - min; }

    interval expand(double delta) const {
        auto padding = delta / 2;
        return interval(min - padding, max + padding);
    }

    bool contains(double x) const { return min <= x && x <= max; }
    bool surrounds(double x) const { return min < x && x < max; }

    double clamp(double x) const {
        if (x < min) return min;
        if (x > max) return max;
        return x;
    }

    static const interval &empty();
    static const interval &universe();

private:
    static double infinity_iv();
};

inline double interval::infinity_iv() {
    return std::numeric_limits<double>::infinity();
}

inline const interval &interval::empty() {
    static interval e(+infinity_iv(), -infinity_iv());
    return e;
}

inline const interval &interval::universe() {
    static interval u(-infinity_iv(), +infinity_iv());
    return u;
}

inline interval operator+(const interval &ival, double displacement) {
    return interval(ival.min + displacement, ival.max + displacement);
}

inline interval operator+(double displacement, const interval &ival) {
    return ival + displacement;
}

#endif // INTERVAL_HPP
