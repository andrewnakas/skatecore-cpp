// Port of skate-core/src/camera/shot_orientation.rs.
#include "s3/core/camera/shot_orientation.hpp"

#include "s3/core/camera/shot.hpp"
#include "s3/core/camera/vector_tracker.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::camera::shot_orientation {

using physics::native_arithmetic::dot4;

F4 interpolate_orientation(F4 from, F4 to, float fraction) { return slerp(from, to, shot::sine_blend(fraction)); }

F4 slerp(F4 from, F4 to, float weight) {
    const float dot = dot4(from, to);
    const F4 signed_from = 0.0f > dot ? rs::map(from, [](float v) { return -v; }) : from;
    const float absolute_dot = 0.0f > dot ? -dot : dot;
    if (absolute_dot > rs::from_bits(0x3f7f069e)) {
        const bool same_direction = dot4(signed_from, to) > 0.0f;
        const F4 value = rs::from_fn<4>([&](std::size_t i) {
            if (same_direction) {
                return rs::mul_add(to[i] - signed_from[i], weight, signed_from[i]);
            }
            return rs::mul_add(-(to[i] + signed_from[i]), weight, signed_from[i]);
        });
        const float square = dot4(value, value);
        float inverse = physics::reciprocal_sqrt::estimate(square);
        for (int k = 0; k < 2; ++k) {
            inverse = rs::mul_add(inverse * 0.5f, rs::mul_add(-square, inverse * inverse, 1.0f), inverse);
        }
        return rs::map(value, [&](float v) { return v * inverse; });
    }
    const float angle = trigonometry::acos(absolute_dot);
    const float inverse_sine = vector_tracker::refined_reciprocal(trigonometry::sin(angle));
    const float first = trigonometry::sin((1.0f - weight) * angle) * inverse_sine;
    const float second = trigonometry::sin(weight * angle) * inverse_sine;
    return rs::from_fn<4>([&](std::size_t i) { return rs::mul_add(to[i], second, signed_from[i] * first); });
}

}  // namespace s3::camera::shot_orientation
