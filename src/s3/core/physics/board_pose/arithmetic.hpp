// Port of skate-core/src/physics/board_pose/arithmetic.rs (private module physics::board_pose::arithmetic).
// Arithmetic order from the retained TU3 pose functions; guest lane order.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/physics/native_arithmetic.hpp"
#include "s3/core/physics/reciprocal_sqrt.hpp"
#include "s3/core/rs.hpp"

namespace s3::physics::board_pose {
/// Guest-order Ri/Up/At/translation vectors, including all packed fourth lanes.
using PoseMatrix = std::array<std::uint32_t, 16>;
}  // namespace s3::physics::board_pose

namespace s3::physics::board_pose::arithmetic {

using V = std::array<float, 4>;
using M = std::array<V, 4>;

inline V splat(float v) { return V{v, v, v, v}; }
inline M matrix(const PoseMatrix& w) {
    M m{};
    for (std::size_t i = 0; i < 4; ++i)
        for (std::size_t j = 0; j < 4; ++j) m[i][j] = rs::from_bits(w[i * 4 + j]);
    return m;
}
inline PoseMatrix words(const M& m) {
    PoseMatrix w{};
    for (std::size_t i = 0; i < 16; ++i) w[i] = rs::to_bits(m[i / 4][i % 4]);
    return w;
}
inline V add(const V& a, const V& b) { return V{a[0] + b[0], a[1] + b[1], a[2] + b[2], a[3] + b[3]}; }
inline V sub(const V& a, const V& b) { return V{a[0] - b[0], a[1] - b[1], a[2] - b[2], a[3] - b[3]}; }
inline V mul(const V& a, const V& b) { return V{a[0] * b[0], a[1] * b[1], a[2] * b[2], a[3] * b[3]}; }
inline V madd(const V& a, const V& b, const V& c) {
    return V{rs::mul_add(a[0], b[0], c[0]), rs::mul_add(a[1], b[1], c[1]),
             rs::mul_add(a[2], b[2], c[2]), rs::mul_add(a[3], b[3], c[3])};
}
inline V nmsub(const V& a, const V& b, const V& c) {
    return V{rs::mul_add(-a[0], b[0], c[0]), rs::mul_add(-a[1], b[1], c[1]),
             rs::mul_add(-a[2], b[2], c[2]), rs::mul_add(-a[3], b[3], c[3])};
}
inline V perm(const V& a, std::array<std::size_t, 4> p) { return V{a[p[0]], a[p[1]], a[p[2]], a[p[3]]}; }
inline float dot(const V& a, const V& b, bool four) {
    if (four) {
        return native_arithmetic::dot4(a, b);
    }
    return native_arithmetic::dot3(a, b);
}
inline float rsqrt(float v) {
    float e = reciprocal_sqrt::estimate(v);
    for (int i = 0; i < 2; ++i) {
        const float s = e * e;
        const float h = e * 0.5f;
        e = rs::mul_add(h, rs::mul_add(-v, s, 1.0f), e);
    }
    return e;
}
inline V normalize(const V& v, bool four) { return mul(v, splat(rsqrt(dot(v, v, four)))); }
inline V cross(const V& a, const V& b) {
    return perm(nmsub(perm(a, {1, 2, 0, 3}), b, mul(a, perm(b, {1, 2, 0, 3}))), {1, 2, 0, 3});
}
inline M compose(const M& a, const M& b) {
    M out{};
    for (std::size_t i = 0; i < 4; ++i) {
        const V first = i == 3 ? madd(splat(b[i][0]), a[0], a[3]) : mul(splat(b[i][0]), a[0]);
        out[i] = madd(splat(b[i][2]), a[2], madd(splat(b[i][1]), a[1], first));
    }
    return out;
}
inline M inverse_rigid(const M& m) {
    M out{};
    for (std::size_t i = 0; i < 3; ++i) out[i] = V{m[0][i], m[1][i], m[2][i], 0.0f};
    const V neg = sub(splat(0.0f), m[3]);
    out[3] = madd(splat(neg[0]), out[0], madd(splat(neg[1]), out[1], mul(splat(neg[2]), out[2])));
    return out;
}

}  // namespace s3::physics::board_pose::arithmetic
