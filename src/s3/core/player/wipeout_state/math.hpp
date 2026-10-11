// Port of skate-core/src/player/wipeout_state/math.rs (whole file).
// Scalar/vector expressions recovered from original Wipeout control kernels.
#pragma once
#include <array>

#include "s3/core/physics/board_motion_output.hpp"
#include "s3/core/physics/native_arithmetic.hpp"

namespace s3::player::wipeout_state::math {

using physics::board_motion_output::inverse_length_squared;
namespace native_arithmetic = physics::native_arithmetic;

using V = std::array<float, 4>;

inline V add(V a, V b) { return V{a[0] + b[0], a[1] + b[1], a[2] + b[2], a[3] + b[3]}; }
inline V sub(V a, V b) { return V{a[0] - b[0], a[1] - b[1], a[2] - b[2], a[3] - b[3]}; }
inline V scale(V a, float b) { return V{a[0] * b, a[1] * b, a[2] * b, a[3] * b}; }
inline V madd(V a, float b, V c) {
    return V{rs::mul_add(a[0], b, c[0]), rs::mul_add(a[1], b, c[1]), rs::mul_add(a[2], b, c[2]),
             rs::mul_add(a[3], b, c[3])};
}
inline float dot(V a, V b) { return native_arithmetic::dot3(a, b); }
inline V cross(V a, V b) {
    return V{
        rs::mul_add(-a[2], b[1], a[1] * b[2]),
        rs::mul_add(-a[0], b[2], a[2] * b[0]),
        rs::mul_add(-a[1], b[0], a[0] * b[1]),
        rs::mul_add(-a[3], b[3], a[3] * b[3]),
    };
}
inline float length(V a) {
    const float square = dot(a, a);
    if (square == 0.0f) {
        return 0.0f;
    }
    return square * inverse_length_squared(square, 2);
}
inline V normalize(V a) { return scale(a, inverse_length_squared(dot(a, a), 2)); }
///Original830BD350 is initialized to splatted358637BD, not a fitted tolerance.
inline V normalize_or(V a, V fallback) {
    if (length(a) > rs::from_bits(0x358637BDu)) {
        return normalize(a);
    }
    return fallback;
}
inline float reciprocal(float v) {
    float inv = native_arithmetic::reciprocal_estimate(v);
    for (int i = 0; i < 2; ++i) {
        inv = rs::mul_add(inv, rs::mul_add(-inv, v, 1.0f), inv);
    }
    return inv;
}
inline float clamp(float v, float low, float high) {
    v = low - v >= 0.0f ? low : v;
    return high - v >= 0.0f ? v : high;
}

///Original82BD3D90: short vectors retain all four lanes.
inline V limit_length(V v, float maximum) {
    const float len = length(v);
    if (!(len >= rs::from_bits(0x37800000u))) {
        return v;
    }
    const float cap = maximum - len >= 0.0f ? len : maximum;
    return scale(scale(v, cap), reciprocal(len));
}

inline float wrap_angle(float angle) {
    const float turns = angle * rs::from_bits(0x3E22F983u);
    const float fraction = turns - rs::floor(turns);
    const float centered = fraction > 0.5f ? fraction - 1.0f : fraction;
    return centered * rs::from_bits(0x40C90FDBu);
}

}  // namespace s3::player::wipeout_state::math
