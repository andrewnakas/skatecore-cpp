// Port of skate-core/src/player/offboard/movement_intent/math.rs (whole file).
// Address-audited scalar forms; host sqrt seed plus original refinements.
// Ordinary PC arithmetic is not bit-exact Xenon emulation.
#pragma once
#include <array>

#include "s3/core/physics/reciprocal_sqrt.hpp"
#include "s3/core/rs.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::player::offboard::movement_intent::math {

using Vector = std::array<float, 4>;
inline const float HALF_PI = rs::from_bits(0x3fc90fdbu);
inline const float EPSILON = rs::from_bits(0x358637bdu);  // 82F826F8 ->830BD350

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
inline float inverse_squared(float value, std::size_t refinements) {
    float r = physics::reciprocal_sqrt::estimate(value);
    for (std::size_t i = 0; i < refinements; ++i) {
        const float error = rs::mul_add(-value, r * r, 1.0f);
        r = rs::mul_add(r * 0.5f, error, r);
    }
    return r;
}
inline float length(Vector v) {
    const float squared = dot(v, v);
    const float value = squared * inverse_squared(squared, 2);
    return squared == 0.0f ? 0.0f : value;
}
inline Vector normalize_or(Vector v, Vector fallback) {
    const float squared = dot(v, v);
    const float r = inverse_squared(squared, 2);
    const float len = squared == 0.0f ? 0.0f : squared * r;
    return len > EPSILON ? scale(v, r) : fallback;
}
/// 8269A4F0 permute [x,zero,x,x], then vrlimi mask2 restores Z.
/// W is X, not zero: preserve the native lane even though the caller dots XYZ.
inline Vector horizontal(Vector v) { return normalize_or(Vector{v[0], 0.0f, v[2], v[0]}, Vector{}); }
inline float wrap_angle(float angle) {
    const float turns = angle * rs::from_bits(0x3e22f983u);
    const float fraction = turns - rs::floor(turns);
    return (fraction - (fraction > 0.5f ? 1.0f : 0.0f)) * rs::from_bits(0x40c90fdbu);
}
inline float signed_angle(Vector a, Vector b, Vector axis) {
    const float aa = dot(a, a);
    const float bb = dot(b, b);
    const float threshold = rs::from_bits(0x38d1b717u);
    if (!(aa > threshold && bb > threshold)) return 0.0f;
    a = scale(a, inverse_squared(aa, 1));
    b = scale(b, inverse_squared(bb, 1));
    const float angle = trigonometry::acos(rs::min(rs::max(dot(a, b), -1.0f), 1.0f));
    const Vector cross{rs::mul_add(-a[2], b[1], a[1] * b[2]), rs::mul_add(-a[0], b[2], a[2] * b[0]),
                       rs::mul_add(-a[1], b[0], a[0] * b[1]), rs::mul_add(-a[3], b[3], a[3] * b[3])};
    return dot(cross, axis) < 0.0f ? rs::from_bits(0x40c90fdbu) - angle : angle;
}
inline float projected_angle(Vector a, Vector b, Vector axis) {
    if (!(dot(a, a) * dot(b, b) > rs::from_bits(0x37800000u))) return 0.0f;
    return signed_angle(sub(a, scale(axis, dot(axis, a))), sub(b, scale(axis, dot(axis, b))), axis);
}

}  // namespace s3::player::offboard::movement_intent::math
