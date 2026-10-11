// Port of skate-core/src/player/offboard/movement_velocity/math.rs (whole file).
// 82BD41B0 delta bound and82C1E170 one-sided projection, original raw audited.
#pragma once
#include <array>
#include <cmath>

#include "s3/core/physics/reciprocal_sqrt.hpp"
#include "s3/core/rs.hpp"

namespace s3::player::offboard::movement_velocity::math {

using Vector = std::array<float, 4>;

inline float select(float test, float yes, float no) { return test >= 0.0f ? yes : no; }
inline float clamp(float value, float low, float high) {
    const float lower = select(low - value, low, value);
    return select(high - lower, lower, high);
}
inline float dot(Vector a, Vector b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
inline Vector scale(Vector a, float s) { return Vector{a[0] * s, a[1] * s, a[2] * s, a[3] * s}; }
inline Vector sub(Vector a, Vector b) { return Vector{a[0] - b[0], a[1] - b[1], a[2] - b[2], a[3] - b[3]}; }
inline Vector madd(Vector a, float s, Vector b) {
    return rs::from_fn<4>([&](std::size_t i) { return rs::mul_add(a[i], s, b[i]); });
}
inline float inverse_squared(float value) {
    // The portable estimate preserves subnormals. For a tiny positive squared
    // length, r*r can overflow even though both the length and inverse root
    // are finite. Normalize its exponent before the recovered refinements;
    // rsqrt(x * 2^24) * 2^12 = rsqrt(x). Normal inputs keep their old path.
    float rescale = 1.0f;
    if (value > 0.0f && std::fpclassify(value) == FP_SUBNORMAL) {
        value = value * 16777216.0f;
        rescale = 4096.0f;
    }
    // PC seed is not bit-exact Xenon; retain both original refinement stages.
    float r = physics::reciprocal_sqrt::estimate(value);
    for (int i = 0; i < 2; ++i) {
        const float error = rs::mul_add(-value, r * r, 1.0f);
        r = rs::mul_add(r * 0.5f, error, r);
    }
    return r * rescale;
}
inline float length(Vector v) {
    const float squared = dot(v, v);
    const float value = squared * inverse_squared(squared);
    return squared == 0.0f ? 0.0f : value;
}
inline Vector normalize_or(Vector v, Vector fallback) {
    const float squared = dot(v, v);
    const float r = inverse_squared(squared);
    const float magnitude = squared == 0.0f ? 0.0f : squared * r;
    return magnitude > rs::from_bits(0x358637bdu) ? scale(v, r) : fallback;
}
inline Vector limit_delta(Vector target, Vector current, float maximum) {
    const Vector delta = sub(target, current);
    const float magnitude = length(delta);
    if (maximum >= magnitude) return target;
    return madd(delta, maximum / magnitude, current);
}
inline Vector remove_positive(Vector value, Vector direction) {
    const float projection = dot(value, normalize_or(direction, Vector{}));
    // Native subtracts original direction, NOT the normalized direction.
    return projection > 0.0f ? sub(value, scale(direction, projection)) : value;
}

}  // namespace s3::player::offboard::movement_velocity::math
