// Port of skate-core/src/physics/hook_drive/frames.rs.
#include "s3/core/physics/hook_drive/frames.hpp"

#include "s3/core/physics/reciprocal_sqrt.hpp"
#include "s3/core/rs.hpp"

namespace s3::physics::hook_drive::frames {

namespace {
using V = std::array<float, 4>;
V add(const V& a, const V& b) { return {a[0] + b[0], a[1] + b[1], a[2] + b[2], a[3] + b[3]}; }
V sub(const V& a, const V& b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2], a[3] - b[3]}; }
V mul(const V& a, const V& b) { return {a[0] * b[0], a[1] * b[1], a[2] * b[2], a[3] * b[3]}; }

V quaternion(const std::array<V, 3>& m) {
    const V& ri = m[0];
    const V& up = m[1];
    const V& at = m[2];
    // Sign masks are initialized by TU3 82F83360/90/C0, not image defaults.
    const V rx{ri[0], ri[0], -ri[0], -ri[0]};
    const V uy{up[1], -up[1], up[1], -up[1]};
    const V az{at[2], -at[2], -at[2], at[2]};
    const V first = add(rx, uy);
    const V d = add(first, add(az, V{1.0f, 1.0f, 1.0f, 1.0f}));
    const V inv = rs::map(d, rsqrt);
    const V roots = mul(d, inv);
    const V halves = mul(V{0.5f, 0.5f, 0.5f, 0.5f}, inv);
    const V a{up[2], at[0], ri[1], 0.5f};
    const V b{at[1], ri[2], up[0], 0.0f};
    const V minus = sub(a, b);
    const V plus = add(a, b);
    const V y = mul(V{plus[2], minus[3], plus[0], minus[1]}, V{halves[2], roots[2], halves[2], halves[2]});
    const V z = mul(V{plus[1], plus[0], minus[3], minus[2]}, V{halves[3], halves[3], roots[3], halves[3]});
    const V x = mul(V{minus[3], plus[2], plus[1], minus[0]}, V{roots[1], halves[1], halves[1], halves[1]});
    const V w = mul(minus, V{halves[0], halves[0], halves[0], roots[0]});
    const V yz = up[1] > at[2] ? y : z;
    const V xyz = (ri[0] > up[1] && ri[0] > at[2]) ? x : yz;
    return first[0] + at[2] > 0.0f ? w : xyz;
}

std::array<std::uint32_t, 4> convert(const std::array<std::uint32_t, 12>& basis) {
    std::array<V, 3> m{};
    for (std::size_t i = 0; i < 3; ++i)
        for (std::size_t j = 0; j < 4; ++j) m[i][j] = rs::from_bits(basis[i * 4 + j]);
    return rs::to_bits(quaternion(m));
}
}  // namespace

float rsqrt(float v) {
    float e = reciprocal_sqrt::estimate(v);
    for (int i = 0; i < 2; ++i) {
        const float s = e * e;
        const float h = e * 0.5f;
        e = rs::mul_add(h, rs::mul_add(-v, s, 1.0f), e);
    }
    return e;
}

void set_child_angular_frame(std::array<std::uint32_t, 16>& frames, std::array<std::uint32_t, 12> basis) {
    const auto q = convert(basis);
    for (std::size_t i = 0; i < 4; ++i) frames[i] = q[i];
}

void set_parent_angular_frame(std::array<std::uint32_t, 16>& frames, std::array<std::uint32_t, 12> basis) {
    const auto q = convert(basis);
    for (std::size_t i = 0; i < 4; ++i) frames[8 + i] = q[i];
}

}  // namespace s3::physics::hook_drive::frames
