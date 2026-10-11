// Port of skate-core/src/player/offboard/ground_sync/math.rs (whole file).
#pragma once
#include <array>

#include "s3/core/physics/reciprocal_sqrt.hpp"
#include "s3/core/rs.hpp"

namespace s3::player::offboard::ground_sync::math {

using Vector = std::array<float, 4>;

inline float dot(Vector a, Vector b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
inline Vector scale(Vector v, float s) { return Vector{v[0] * s, v[1] * s, v[2] * s, v[3] * s}; }
inline Vector sub(Vector a, Vector b) { return Vector{a[0] - b[0], a[1] - b[1], a[2] - b[2], a[3] - b[3]}; }
inline Vector madd(Vector a, float s, Vector b) {
    return rs::from_fn<4>([&](std::size_t i) { return rs::mul_add(a[i], s, b[i]); });
}
inline Vector cross(Vector a, Vector b) {
    return Vector{rs::mul_add(-a[2], b[1], a[1] * b[2]), rs::mul_add(-a[0], b[2], a[2] * b[0]),
                  rs::mul_add(-a[1], b[0], a[0] * b[1]), rs::mul_add(-a[3], b[3], a[3] * b[3])};
}
inline float inverse(float sq) {
    // PC reciprocal-square-root seed; two native Newton refinements retained.
    float r = physics::reciprocal_sqrt::estimate(sq);
    for (int i = 0; i < 2; ++i) r = rs::mul_add(r * 0.5f, rs::mul_add(-sq, r * r, 1.0f), r);
    return r;
}
inline Vector normalize_unchecked(Vector v) { return scale(v, inverse(dot(v, v))); }
inline Vector normalize_or(Vector v, Vector fallback) {
    const float sq = dot(v, v);
    const float r = inverse(sq);
    const float length = sq == 0.0f ? 0.0f : sq * r;
    return length > rs::from_bits(0x358637bdu) ? scale(v, r) : fallback;
}

}  // namespace s3::player::offboard::ground_sync::math
