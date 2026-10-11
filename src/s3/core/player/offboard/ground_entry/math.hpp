// Port of skate-core/src/player/offboard/ground_entry/math.rs (whole file).
// Original82C1E220 quaternion vector rotation and8296EC98 signed angle.
#pragma once
#include <array>
#include <cstddef>

#include "s3/core/physics/native_arithmetic.hpp"
#include "s3/core/physics/reciprocal_sqrt.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::player::offboard::ground_entry::math {

using Vector = std::array<float, 4>;
using Frame = std::array<Vector, 4>;

inline const float TAU = rs::from_bits(0x40c90fdbu);
inline constexpr Frame IDENTITY{{{1.0f, 0.0f, 0.0f, 0.0f},
                                 {0.0f, 1.0f, 0.0f, 0.0f},
                                 {0.0f, 0.0f, 1.0f, 0.0f},
                                 {0.0f, 0.0f, 0.0f, 0.0f}}};

inline float dot(Vector a, Vector b) { return physics::native_arithmetic::dot3(a, b); }
inline Vector scale(Vector v, float s) { return Vector{v[0] * s, v[1] * s, v[2] * s, v[3] * s}; }
inline Vector sub(Vector a, Vector b) { return Vector{a[0] - b[0], a[1] - b[1], a[2] - b[2], a[3] - b[3]}; }
inline Vector cross(Vector a, Vector b) {
    return Vector{
        rs::mul_add(-a[2], b[1], a[1] * b[2]),
        rs::mul_add(-a[0], b[2], a[2] * b[0]),
        rs::mul_add(-a[1], b[0], a[0] * b[1]),
        rs::mul_add(-a[3], b[3], a[3] * b[3]),
    };
}
inline float inverse_length(float q, std::size_t refinements) {
    float r = physics::reciprocal_sqrt::estimate(q);
    for (std::size_t i = 0; i < refinements; ++i) {
        r = rs::mul_add(r * 0.5f, rs::mul_add(-q, r * r, 1.0f), r);
    }
    return r;
}
inline float length(Vector v) {
    const float q = dot(v, v);
    const float l = q * inverse_length(q, 2);
    return q == 0.0f ? 0.0f : l;
}
inline float wrap(float angle) {
    const float turns = angle * rs::from_bits(0x3e22f983u);
    const float fraction = turns - rs::floor(turns);
    return (fraction - (fraction > 0.5f ? 1.0f : 0.0f)) * TAU;
}
inline float signed_angle(Vector a, Vector b, Vector axis) {
    const float qa = dot(a, a);
    const float qb = dot(b, b);
    if (!(qa > rs::from_bits(0x38d1b717u)) || !(qb > rs::from_bits(0x38d1b717u))) {
        return 0.0f;
    }
    a = scale(a, inverse_length(qa, 1));
    b = scale(b, inverse_length(qb, 1));
    const float cosine = rs::min(rs::max(dot(a, b), -1.0f), 1.0f);
    const float angle = trigonometry::acos(cosine);
    if (dot(cross(a, b), axis) < 0.0f) {
        return TAU - angle;
    }
    return angle;
}
inline Vector rotate(Vector v, Vector axis, float angle) {
    const auto sc = trigonometry::sin_cos(angle * 0.5f);
    const float s = sc.first, c = sc.second;
    Vector q = scale(axis, s);
    q[3] = c;
    const Vector first = cross(q, v);
    const Vector intermediate = rs::from_fn<4>([&](std::size_t i) { return rs::mul_add(v[i], c, first[i]); });
    const Vector second = cross(q, intermediate);
    return rs::from_fn<4>([&](std::size_t i) { return rs::mul_add(second[i], 2.0f, v[i]); });
}

}  // namespace s3::player::offboard::ground_entry::math
