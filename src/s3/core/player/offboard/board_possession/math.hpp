// Port of skate-core/src/player/offboard/board_possession/math.rs (whole file).
// VMX arithmetic shared by the possession stages. Four stored lanes, xyz dot.
#pragma once
#include <array>
#include <cstddef>

#include "s3/core/input/angle.hpp"
#include "s3/core/physics/native_arithmetic.hpp"
#include "s3/core/physics/reciprocal_sqrt.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::player::offboard::board_possession::math {

using Vector = std::array<float, 4>;
using Frame = std::array<Vector, 4>;

inline float dot(Vector a, Vector b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
inline Vector sub(Vector a, Vector b) { return Vector{a[0] - b[0], a[1] - b[1], a[2] - b[2], a[3] - b[3]}; }
inline Vector scale(Vector a, float s) { return Vector{a[0] * s, a[1] * s, a[2] * s, a[3] * s}; }
inline Vector madd(Vector a, float s, Vector b) {
    return Vector{rs::mul_add(a[0], s, b[0]), rs::mul_add(a[1], s, b[1]), rs::mul_add(a[2], s, b[2]),
                  rs::mul_add(a[3], s, b[3])};
}
inline Vector cross(Vector a, Vector b) {
    return Vector{
        rs::mul_add(-a[2], b[1], a[1] * b[2]),
        rs::mul_add(-a[0], b[2], a[2] * b[0]),
        rs::mul_add(-a[1], b[0], a[0] * b[1]),
        rs::mul_add(-a[3], b[3], a[3] * b[3]),
    };
}
namespace detail {
inline float inverse_sqrt(float s) {
    float r = physics::reciprocal_sqrt::estimate(s);
    for (int i = 0; i < 2; ++i) {
        r = rs::mul_add(r * 0.5f, rs::mul_add(-s, r * r, 1.0f), r);
    }
    return r;
}
}  // namespace detail
inline float length(Vector a) {
    const float s = dot(a, a);
    const float n = s * detail::inverse_sqrt(s);
    return s == 0.0f ? 0.0f : n;
}
inline Vector unit(Vector a, Vector fallback) {
    const float s = dot(a, a);
    const float r = detail::inverse_sqrt(s);
    const float n = s == 0.0f ? 0.0f : s * r;
    if (n > rs::from_bits(0x358637bdu)) {
        return scale(a, r);
    }
    return fallback;
}
inline float reciprocal(float x, std::size_t steps) {
    float r = physics::native_arithmetic::reciprocal_estimate(x);
    for (std::size_t i = 0; i < steps; ++i) {
        r = rs::mul_add(r, rs::mul_add(-r, x, 1.0f), r);
    }
    return r;
}
///76D20 differs from atan2 at zero, including its signed-zero half-pi result.
inline float quadrant_angle(float y, float x) {
    const float basic = input::angle::atan(rs::mul_add(y, reciprocal(x, 1), 0.0f));
    const std::uint32_t sign = rs::to_bits(y) & 0x80000000u;
    const float result = 0.0f > x ? rs::from_bits(0x40490fdbu | sign) + basic : basic;
    if (x == 0.0f) {
        return rs::from_bits(0x3fc90fdbu | sign);
    }
    return result;
}
inline Vector transform_direction(const Frame& frame, Vector v) {
    return madd(frame[2], v[2], madd(frame[1], v[1], scale(frame[0], v[0])));
}
inline float projected_angle(Vector a, Vector b, Vector axis) {
    if (!(dot(a, a) * dot(b, b) > rs::from_bits(0x37800000u))) {
        return 0.0f;
    }
    a = sub(a, scale(axis, dot(axis, a)));
    b = sub(b, scale(axis, dot(axis, b)));
    const float aa = dot(a, a);
    const float bb = dot(b, b);
    const float gate = rs::from_bits(0x38d1b717u);
    if (!(aa > gate && bb > gate)) {
        return 0.0f;
    }
    auto one = [](float x) {
        const float r = physics::reciprocal_sqrt::estimate(x);
        return rs::mul_add(r * 0.5f, rs::mul_add(-x, r * r, 1.0f), r);
    };
    a = scale(a, one(aa));
    b = scale(b, one(bb));
    const float angle = trigonometry::acos(rs::min(rs::max(dot(a, b), -1.0f), 1.0f));
    if (dot(cross(a, b), axis) < 0.0f) {
        return rs::from_bits(0x40c90fdbu) - angle;
    }
    return angle;
}
inline float wrap(float angle) {
    const float t = angle * rs::from_bits(0x3e22f983u);
    const float fraction = t - rs::floor(t);
    return (fraction - (fraction > 0.5f ? 1.0f : 0.0f)) * rs::from_bits(0x40c90fdbu);
}

}  // namespace s3::player::offboard::board_possession::math
