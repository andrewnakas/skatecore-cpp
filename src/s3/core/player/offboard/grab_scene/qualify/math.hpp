// Port of skate-core/src/player/offboard/grab_scene/qualify/math.rs (whole file).
// C++ namespace `qualify_`: Rust has both fn `qualify` and module `qualify`.
// Scalar forms of original VMX steps; independent PC estimates, not bit parity.
#pragma once
#include <array>

#include "s3/core/physics/reciprocal_sqrt.hpp"
#include "s3/core/rs.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::player::offboard::grab_scene::qualify_::math {

using Vector = std::array<float, 4>;
inline float dot(Vector a, Vector b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
inline Vector sub(Vector a, Vector b) { return Vector{a[0] - b[0], a[1] - b[1], a[2] - b[2], a[3] - b[3]}; }
inline Vector flatten(Vector a) {
    a[1] = 0.0f;
    return a;
}
inline Vector cross(Vector a, Vector b) {
    return Vector{rs::mul_add(-a[2], b[1], a[1] * b[2]), rs::mul_add(-a[0], b[2], a[2] * b[0]),
                  rs::mul_add(-a[1], b[0], a[0] * b[1]), rs::mul_add(-a[3], b[3], a[3] * b[3])};
}
inline float inv_sqrt(float value, std::size_t steps) {
    float r = physics::reciprocal_sqrt::estimate(value);
    for (std::size_t i = 0; i < steps; ++i) r = rs::mul_add(r * 0.5f, rs::mul_add(-value, r * r, 1.0f), r);
    return r;
}
inline float reciprocal(float value) {
    float r = 1.0f / value;
    for (int i = 0; i < 2; ++i) r = rs::mul_add(r, rs::mul_add(-value, r, 1.0f), r);
    return r;
}
inline float length(Vector v) {
    const float squared = dot(v, v);
    const float result = squared * inv_sqrt(squared, 2);
    return squared == 0.0f ? 0.0f : result;
}
inline Vector normalize(Vector v) {
    const float squared = dot(v, v);
    const float r = inv_sqrt(squared, 2);
    const float len = squared == 0.0f ? 0.0f : squared * r;
    if (len > rs::from_bits(0x358637bdu)) return rs::map(v, [&](float x) { return x * r; });
    return Vector{};
}
/// 8296EBB0: separate squared-length guards and ONE refinement before acos.
inline float angle(Vector a, Vector b) {
    const float aa = dot(a, a);
    const float bb = dot(b, b);
    if (!(aa > rs::from_bits(0x38d1b717u) && bb > rs::from_bits(0x38d1b717u))) return 0.0f;
    a = rs::map(a, [&](float x) { return x * inv_sqrt(aa, 1); });
    b = rs::map(b, [&](float x) { return x * inv_sqrt(bb, 1); });
    return trigonometry::acos(rs::min(rs::max(dot(a, b), -1.0f), 1.0f));
}
/// 82E09D28 retains unnormalized direction below its native LENGTH threshold.
inline Vector closest_point(Vector position, const std::array<Vector, 2>& ab) {
    const Vector a = ab[0], b = ab[1];
    Vector direction = sub(b, a);
    const float len = length(direction);
    if (len > rs::from_bits(0x37800000u)) {
        const float r = reciprocal(len);
        direction = rs::map(direction, [&](float v) { return v * r; });
    }
    const float along = rs::max(rs::min(dot(direction, sub(position, a)), len), 0.0f);
    return rs::from_fn<4>([&](std::size_t i) { return rs::mul_add(direction[i], along, a[i]); });
}
inline Vector transform(const std::array<Vector, 4>& frame, Vector point) {
    return rs::from_fn<4>([&](std::size_t i) {
        return rs::mul_add(frame[2][i], point[2],
                           rs::mul_add(frame[1][i], point[1], rs::mul_add(frame[0][i], point[0], frame[3][i])));
    });
}
inline Vector inverse_point(const std::array<Vector, 4>& frame, Vector point) {
    Vector result{};
    for (std::size_t i = 0; i < 3; ++i) {
        const float translation = rs::mul_add(frame[i][0], -frame[3][0],
                                              rs::mul_add(frame[i][1], -frame[3][1], frame[i][2] * -frame[3][2]));
        result[i] = rs::mul_add(frame[i][2], point[2],
                                rs::mul_add(frame[i][1], point[1], rs::mul_add(frame[i][0], point[0], translation)));
    }
    return result;
}

}  // namespace s3::player::offboard::grab_scene::qualify_::math
