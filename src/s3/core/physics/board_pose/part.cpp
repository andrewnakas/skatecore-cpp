// Port of skate-core/src/physics/board_pose/part.rs.
#include "s3/core/physics/board_pose/part.hpp"

#include "s3/core/physics/board_pose/normalization.hpp"

namespace s3::physics::board_pose::part {

using namespace arithmetic;

namespace {

V quaternion(const M& m) {
    const V& ri = m[0];
    const V& up = m[1];
    const V& at = m[2];
    // Sign masks are initialized by TU3 82F83360/90/C0, not image defaults.
    const V rx{ri[0], ri[0], -ri[0], -ri[0]};
    const V uy{up[1], -up[1], up[1], -up[1]};
    const V az{at[2], -at[2], -at[2], at[2]};
    const V first = add(rx, uy);
    const V d = add(first, add(az, splat(1.0f)));
    const V inv = rs::map(d, [](float x) { return rsqrt(x); });
    const V roots = mul(d, inv);
    const V halves = mul(splat(0.5f), inv);
    const V a{up[2], at[0], ri[1], 0.5f};
    const V b{at[1], ri[2], up[0], 0.0f};
    const V minus = sub(a, b);
    const V plus = add(a, b);
    const V y = mul(V{plus[2], minus[3], plus[0], minus[1]},
                    V{halves[2], roots[2], halves[2], halves[2]});
    const V z = mul(V{plus[1], plus[0], minus[3], minus[2]},
                    V{halves[3], halves[3], roots[3], halves[3]});
    const V x = mul(V{minus[3], plus[2], plus[1], minus[0]},
                    V{roots[1], halves[1], halves[1], halves[1]});
    const V w = mul(minus, V{halves[0], halves[0], halves[0], roots[0]});
    const V yz = up[1] > at[2] ? y : z;
    const V xyz = (ri[0] > up[1] && ri[0] > at[2]) ? x : yz;
    return normalize(first[0] + at[2] > 0.0f ? w : xyz, true);
}

V load(const std::array<std::uint32_t, 44>& w, std::size_t off) {
    return V{rs::from_bits(w[off]), rs::from_bits(w[off + 1]), rs::from_bits(w[off + 2]),
             rs::from_bits(w[off + 3])};
}

void store_xyz(std::array<std::uint32_t, 44>& w, std::size_t off, const V& v) {
    for (std::size_t i = 0; i < 3; ++i) w[off + i] = rs::to_bits(v[i]);
}

}  // namespace

PoseMatrix part_transform(const PartPose& part) {
    if (!part.body) {
        return part.transform;
    }
    const auto& body = *part.body;
    const M m{load(body, 16), load(body, 20), load(body, 24), load(body, 4)};
    return words(part.local_mass_frame ? compose(m, matrix(*part.local_mass_frame)) : m);
}

void set_part_transform(PartPose& part, PoseMatrix requested) {
    if (part.body) {
        auto& body = *part.body;
        const PoseMatrix mw =
            part.local_mass_frame
                ? words(compose(matrix(requested), inverse_rigid(matrix(*part.local_mass_frame))))
                : requested;
        const M m = matrix(normalization::orthonormalize_part_basis(mw));
        const std::pair<std::size_t, V> stores[] = {{16, m[0]}, {20, m[1]}, {24, m[2]}, {4, m[3]}};
        for (const auto& [off, v] : stores) {
            store_xyz(body, off, v);
        }
        const V q = quaternion(m);
        for (std::size_t i = 0; i < 4; ++i) body[i] = rs::to_bits(q[i]);
        if (part.inertia) {
            const auto& inertia = *part.inertia;
            const V& ri = m[0];
            const V& up = m[1];
            const V& at = m[2];
            const V r = mul(ri, splat(rs::from_bits(inertia[0])));
            const V u = mul(up, splat(rs::from_bits(inertia[1])));
            const V a = mul(at, splat(rs::from_bits(inertia[2])));
            const V full = madd(a, splat(at[0]), madd(r, splat(ri[0]), mul(u, splat(up[0]))));
            auto p = [](const V& v) { return perm(v, {2, 1, 1, 3}); };
            auto s = [](const V& v) { return perm(v, {2, 1, 2, 3}); };
            const V split = madd(p(at), s(a), madd(p(ri), s(r), mul(p(up), s(u))));
            store_xyz(body, 28, full);
            store_xyz(body, 32, split);
            body[31] = inertia[4];
        }
    }
    part.transform = requested;
}

}  // namespace s3::physics::board_pose::part
