// Port of skate-core/src/air/trajectory/grind_surface/orientation.rs.
#include "s3/core/air/trajectory/grind_surface/orientation.hpp"

#include "s3/core/air/trajectory/math.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::air::trajectory::grind_surface::orientation {

using namespace trajectory::math;

V tilted_normal(V direction, V up, std::array<float, 2> limits) {
    const float lo = limits[0], hi = limits[1];
    const float pi = rs::from_bits(0x40490fdbu);
    float angle;
    if (lo < hi) {
        if (lo < pi && hi > pi) {
            return up;
        }
        angle = rs::abs(pi - lo) < rs::abs(pi - hi) ? lo : hi;
    } else {
        angle = (hi + lo) * 0.5f;
    }
    return rotate(direction, scale(up, -1.0f), angle);
}

V rotate(V axis, V value, float angle) {
    const auto [sin, cos] = trigonometry::sin_cos(angle * 0.5f);
    const V q = scale(axis, sin);
    return madd(cross(q, madd(value, cos, cross(q, value))), 2.0f, value);
}

}  // namespace s3::air::trajectory::grind_surface::orientation
