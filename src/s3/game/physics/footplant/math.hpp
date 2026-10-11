// Port of skate-game/src/physics/footplant/math.rs.
//! Original vector operation order from82D6FD60/82D6F5F0/82D70070.
//! Estimate seeds use independent PC math; Xenon bit equality is unverified.
#pragma once
#include <array>

#include "s3/core/rs.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::footplant::math {

using V = std::array<float, 4>;

inline constexpr float STEP = rs::from_bits(0x3c888889u);
inline float dot(V a, V b) { return (a[0] * b[0] + a[1] * b[1]) + a[2] * b[2]; }
inline V cross(V a, V b) {
    return {
        rs::mul_add(a[1], b[2], -a[2] * b[1]),
        rs::mul_add(a[2], b[0], -a[0] * b[2]),
        rs::mul_add(a[0], b[1], -a[1] * b[0]),
        0.0f,
    };
}
inline V add(V a, V b) { return {a[0] + b[0], a[1] + b[1], a[2] + b[2], a[3] + b[3]}; }
inline V sub(V a, V b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2], a[3] - b[3]}; }
inline V scale(V a, float s) { return {a[0] * s, a[1] * s, a[2] * s, a[3] * s}; }
inline V madd(V a, float s, V b) {
    return {rs::mul_add(a[0], s, b[0]), rs::mul_add(a[1], s, b[1]), rs::mul_add(a[2], s, b[2]),
            rs::mul_add(a[3], s, b[3])};
}
inline float reciprocal(float s) {
    float r = rs::recip(s);
    for (int i = 0; i < 2; ++i) {
        r = rs::mul_add(rs::mul_add(-s, r, 1.0f), r, r);
    }
    return r;
}
/// Private in Rust.
inline float rsqrt(float s) {
    float r = rs::recip(rs::sqrt(s));
    for (int i = 0; i < 2; ++i) {
        r = rs::mul_add(r * 0.5f, rs::mul_add(-s, r * r, 1.0f), r);
    }
    return r;
}
inline float length(V a) {
    const float s = dot(a, a);
    const float l = s * rsqrt(s);
    return s == 0.0f ? 0.0f : l;
}
inline V normalize(V a) {
    const float s = dot(a, a);
    const float r = rsqrt(s);
    const float l = s == 0.0f ? 0.0f : s * r;
    if (l > rs::from_bits(0x358637bdu)) {
        return scale(a, r);
    }
    return V{};
}
inline V point(const std::array<V, 4>& m, V p) {
    V out{};
    for (std::size_t i = 0; i < 4; ++i) {
        const float x = rs::mul_add(m[0][i], p[0], m[3][i]);
        const float y = rs::mul_add(m[1][i], p[1], x);
        out[i] = rs::mul_add(m[2][i], p[2], y);
    }
    return out;
}
inline V rotate(const std::array<V, 4>& m, V v) {
    V out{};
    for (std::size_t i = 0; i < 4; ++i) {
        const float x = m[0][i] * v[0];
        const float y = rs::mul_add(m[1][i], v[1], x);
        out[i] = rs::mul_add(m[2][i], v[2], y);
    }
    return out;
}
inline V clamp_length(V v, float maximum) {
    const float l = length(v);
    if (!(l >= rs::from_bits(0x37800000u))) {
        return v;
    }
    const float capped = maximum - l >= 0.0f ? l : maximum;
    return scale(scale(v, capped), reciprocal(l));
}
inline float clamp01(float v) {
    const float a = -v >= 0.0f ? 0.0f : v;
    return 1.0f - a >= 0.0f ? a : 1.0f;
}

}  // namespace s3::game::physics::footplant::math
