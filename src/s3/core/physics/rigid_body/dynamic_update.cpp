// Port of skate-core/src/physics/rigid_body/dynamic_update.rs.
// RigidBody::DynamicUpdate82AE6590 reconstruction candidate.
#include "s3/core/physics/rigid_body.hpp"
#include "s3/core/physics/native_arithmetic.hpp"
#include "s3/core/physics/reciprocal_sqrt.hpp"

namespace s3::physics::rigid_body::dynamic_update {

namespace {

float refined_rsqrt(float value) {
    float estimate = reciprocal_sqrt::estimate(value);
    for (int i = 0; i < 2; ++i) {
        const float square = estimate * estimate;
        const float half_estimate = estimate * 0.5f;
        const float residual = rs::mul_add(-value, square, 1.0f);
        estimate = rs::mul_add(half_estimate, residual, estimate);
    }
    return estimate;
}

float dot(const V& a, const V& b, bool four) {
    if (four) {
        return native_arithmetic::dot4(a, b);
    }
    return native_arithmetic::dot3(a, b);
}

template <std::size_t N>
V load(const std::array<std::uint32_t, N>& words, std::size_t offset) {
    return V{rs::from_bits(words[offset]), rs::from_bits(words[offset + 1]),
             rs::from_bits(words[offset + 2]), rs::from_bits(words[offset + 3])};
}
template <std::size_t N>
void store_xyz(std::array<std::uint32_t, N>& words, std::size_t offset, const V& value) {
    for (std::size_t i = 0; i < 3; ++i) {
        words[offset + i] = rs::to_bits(value[i]);
    }
}
V perm(const V& value, std::array<std::size_t, 4> indices) {
    return V{value[indices[0]], value[indices[1]], value[indices[2]], value[indices[3]]};
}
V splat(float v) { return V{v, v, v, v}; }
V mul(const V& a, const V& b) { return V{a[0] * b[0], a[1] * b[1], a[2] * b[2], a[3] * b[3]}; }
V add(const V& a, const V& b) { return V{a[0] + b[0], a[1] + b[1], a[2] + b[2], a[3] + b[3]}; }
V sub(const V& a, const V& b) { return V{a[0] - b[0], a[1] - b[1], a[2] - b[2], a[3] - b[3]}; }
V madd(const V& a, const V& b, const V& c) {
    return V{rs::mul_add(a[0], b[0], c[0]), rs::mul_add(a[1], b[1], c[1]),
             rs::mul_add(a[2], b[2], c[2]), rs::mul_add(a[3], b[3], c[3])};
}
V nmsub(const V& a, const V& b, const V& c) {
    return V{rs::mul_add(-a[0], b[0], c[0]), rs::mul_add(-a[1], b[1], c[1]),
             rs::mul_add(-a[2], b[2], c[2]), rs::mul_add(-a[3], b[3], c[3])};
}

std::pair<V, float> cap(const V& value, float maximum) {
    const float squared = dot(value, value, false);
    const float maximum_squared = maximum * maximum;
    // S3 82AE6888/68FC and S2 82AE478C/4804 skip on GT clear (ble),
    // including unordered. Thus ordered >, not !(<=), enters the cap path.
    if (squared > maximum_squared) {
        const float ratio = maximum_squared / squared;
        const float root = ratio * refined_rsqrt(ratio);
        const float scale = ratio == 0.0f ? 0.0f : root;
        return {mul(value, splat(scale)), maximum_squared};
    }
    return {value, squared};
}

}  // namespace

DynamicUpdateResult dynamic_update_packed(std::array<std::uint32_t, 44>& body,
                                          const std::array<std::uint32_t, 10>& inertia,
                                          RetailSimulationStep simulation,
                                          std::array<std::uint32_t, 16>& reactions) {
    const V dt = splat(simulation.time_step);
    const V linear = madd(madd(load(body, 36), dt, load(body, 8)), dt, load(reactions, 0));
    const V angular = madd(madd(load(body, 40), dt, load(body, 12)), dt, load(reactions, 8));
    const V position = add(add(load(body, 4), load(reactions, 4)), linear);
    store_xyz(body, 4, position);
    const V rotation = add(angular, load(reactions, 12));
    const V q = orientation(load(body, 0), rotation);
    for (std::size_t i = 0; i < 4; ++i) body[i] = rs::to_bits(q[i]);
    const std::array<V, 3> basis = quaternion_basis(q);
    const std::size_t offsets[3] = {16, 20, 24};
    for (std::size_t i = 0; i < 3; ++i) store_xyz(body, offsets[i], basis[i]);
    const std::array<std::uint32_t, 3> tensor{inertia[0], inertia[1], inertia[2]};
    const auto [full, split] = inverse_inertia(basis, tensor);
    store_xyz(body, 28, full);
    store_xyz(body, 32, split);
    auto rate = [&](std::uint32_t drag) {
        const float difference = simulation.frequency - rs::from_bits(drag);
        return difference > 0.0f ? difference : 0.0f;
    };
    const auto [omega, angular_squared] =
        cap(mul(angular, splat(rate(inertia[9]))), rs::from_bits(inertia[7]));
    const auto [velocity, linear_squared] =
        cap(mul(linear, splat(rate(inertia[8]))), rs::from_bits(inertia[6]));
    // Scalar fmuls followed by fmadds; cap branches publish the squared cap,
    // rather than recalculating squared speed from the rounded scaled vector.
    const float factor = rs::from_bits(inertia[5]) * rs::from_bits(body[31]);
    const float energy = rs::mul_add(factor, angular_squared, linear_squared);
    // S3 82AE6974 / S2 82AE487C: bge tests LT clear, including unordered.
    // Do not replace this with >=; the host predicate below is intentional.
    if (!(energy < simulation.minimum_energy)) {
        body[43] = 0;
    } else {
        const std::uint32_t cooldown =
            !(energy > rs::from_bits(body[39])) ? body[43] + 1u : body[43];
        body[43] = std::min(cooldown, simulation.cool_down);
    }
    body[39] = rs::to_bits(energy);
    store_xyz(body, 8, velocity);
    store_xyz(body, 12, omega);
    reactions.fill(0);
    const Vector3 gravity = simulation.gravity_acceleration;
    store_xyz(body, 36, V{gravity.x, gravity.y, gravity.z, 0.0f});
    store_xyz(body, 40, splat(0.0f));
    return DynamicUpdateResult{
        {rotation[0], rotation[1], rotation[2]},
        linear_squared,
        angular_squared,
    };
}

V orientation(V q, V rotation) {
    // S3 82AE6668..66C8 / S2 82AE455C..45C8: cross product then normalization.
    const V cross = perm(
        nmsub(perm(rotation, {1, 2, 0, 3}), q, mul(rotation, perm(q, {1, 2, 0, 3}))), {1, 2, 0, 3});
    V increment = mul(madd(rotation, splat(q[3]), cross), splat(0.5f));
    increment[3] = dot(rotation, q, false) * -0.5f;
    const V candidate = add(q, increment);
    return mul(candidate, splat(refined_rsqrt(dot(candidate, candidate, true))));
}

std::array<V, 3> quaternion_basis(V q) {
    // 82139A00 contains sqrt(2) rounded to binary32. Replacing these products
    // with 2*q_i*q_j changes the native basis words.
    const V s = mul(q, splat(rs::from_bits(0x3FB504F3u)));
    const V d = nmsub(s, s, splat(0.5f));
    const V diagonal = add(d, perm(d, {1, 2, 0, 0}));
    const V products = mul(s, perm(s, {1, 2, 0, 0}));
    const V w_products = mul(splat(s[3]), perm(s, {2, 0, 1, 0}));
    const V plus = add(products, w_products);
    const V minus = sub(products, w_products);
    return {
        V{diagonal[1], plus[0], minus[2], 0.0f},
        V{minus[0], diagonal[2], plus[1], 0.0f},
        V{plus[2], minus[1], diagonal[0], 0.0f},
    };
}

std::pair<V, V> inverse_inertia(std::array<V, 3> basis, std::array<std::uint32_t, 3> tensor) {
    const V& ri = basis[0];
    const V& up = basis[1];
    const V& at = basis[2];
    const V r = mul(ri, splat(rs::from_bits(tensor[0])));
    const V u = mul(up, splat(rs::from_bits(tensor[1])));
    const V a = mul(at, splat(rs::from_bits(tensor[2])));
    const V full = madd(a, splat(at[0]), madd(r, splat(ri[0]), mul(u, splat(up[0]))));
    auto p = [](const V& v) { return perm(v, {2, 1, 1, 3}); };
    auto s = [](const V& v) { return perm(v, {2, 1, 2, 3}); };
    const V split = madd(p(at), s(a), madd(p(ri), s(r), mul(p(up), s(u))));
    return {full, split};
}

}  // namespace s3::physics::rigid_body::dynamic_update
