// Port of skate-core/src/player/offboard/surface_frame/math.rs (whole file).
// Original82BD3E78/8296EBB0 and inlined VMX arithmetic.
#pragma once
#include <array>

#include "s3/core/physics/reciprocal_sqrt.hpp"
#include "s3/core/rs.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::player::offboard::surface_frame::math {

using Vector = std::array<float, 4>;

inline float select(float test, float yes, float no) { return test >= 0.0f ? yes : no; }
inline float clamp(float v, float low, float high) {
    v = select(low - v, low, v);
    return select(high - v, v, high);
}
inline float dot(Vector a, Vector b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
inline Vector scale(Vector v, float s) { return Vector{v[0] * s, v[1] * s, v[2] * s, v[3] * s}; }
inline Vector add(Vector a, Vector b) { return Vector{a[0] + b[0], a[1] + b[1], a[2] + b[2], a[3] + b[3]}; }
inline Vector sub(Vector a, Vector b) { return Vector{a[0] - b[0], a[1] - b[1], a[2] - b[2], a[3] - b[3]}; }
inline Vector madd(Vector a, float s, Vector b) {
    return rs::from_fn<4>([&](std::size_t i) { return rs::mul_add(a[i], s, b[i]); });
}
inline Vector cross(Vector a, Vector b) {
    // vpermwi0x63 preserves lane w; its fused subtraction is retained too.
    return Vector{rs::mul_add(-a[2], b[1], a[1] * b[2]), rs::mul_add(-a[0], b[2], a[2] * b[0]),
                  rs::mul_add(-a[1], b[0], a[0] * b[1]), rs::mul_add(-a[3], b[3], a[3] * b[3])};
}
inline float inverse(float value, std::size_t steps) {
    // Ordinary host seed, not a claim of bit-exact Xenon estimate emulation.
    float r = physics::reciprocal_sqrt::estimate(value);
    for (std::size_t i = 0; i < steps; ++i) {
        const float e = rs::mul_add(-value, r * r, 1.0f);
        r = rs::mul_add(r * 0.5f, e, r);
    }
    return r;
}
inline float square_root(float value) {
    const float result = value * inverse(value, 2);
    return value == 0.0f ? 0.0f : result;
}
inline float length(Vector v) { return square_root(dot(v, v)); }
inline Vector normalize_or(Vector v, Vector fallback) {
    const float sq = dot(v, v);
    const float r = inverse(sq, 2);
    const float len = sq == 0.0f ? 0.0f : sq * r;
    return len > rs::from_bits(0x358637bdu) ? scale(v, r) : fallback;
}
inline float unsigned_angle(Vector a, Vector b) {
    const float aa = dot(a, a);
    const float bb = dot(b, b);
    const float gate = rs::from_bits(0x38d1b717u);
    if (!(aa > gate && bb > gate)) return 0.0f;
    return trigonometry::acos(rs::min(rs::max(dot(scale(a, inverse(aa, 1)), scale(b, inverse(bb, 1))), -1.0f), 1.0f));
}
inline float wrap_angle(float a) {
    const float t = a * rs::from_bits(0x3e22f983u);
    const float f = t - rs::floor(t);
    return (f - (f > 0.5f ? 1.0f : 0.0f)) * rs::from_bits(0x40c90fdbu);
}
inline Vector clamp_axis(Vector value, Vector axis, float low, float high) {
    const float projection = dot(value, axis);
    return madd(axis, clamp(projection, low, high), sub(value, scale(axis, projection)));
}
/// 82BD3E78 returns original target when close or cross-axis degenerate.
inline Vector clamp_angle(Vector target, Vector reference, float limit) {
    const Vector a = normalize_or(target, Vector{});
    const Vector b = normalize_or(reference, Vector{});
    const Vector axis = cross(a, b);
    const float angle = trigonometry::acos(rs::min(rs::max(dot(a, b), -1.0f), 1.0f));
    const float turns = rs::floor(rs::mul_add(angle, rs::from_bits(0x3e22f983u), 0.5f));
    const float wrapped = rs::mul_add(-turns, rs::from_bits(0x40c90fdbu), angle);
    if (rs::abs(wrapped) < limit || rs::from_bits(0x37800000u) > dot(axis, axis)) return target;
    const Vector unit = scale(axis, inverse(dot(axis, axis), 2));
    const auto [sin, cos] = trigonometry::sin_cos((-limit) * 0.5f);
    Vector q = scale(unit, sin);
    q[3] = cos;
    const Vector first = cross(q, reference);
    const Vector middle = madd(reference, cos, first);
    const Vector rotated = madd(cross(q, middle), 2.0f, reference);
    return scale(rotated, length(target));
}

}  // namespace s3::player::offboard::surface_frame::math
