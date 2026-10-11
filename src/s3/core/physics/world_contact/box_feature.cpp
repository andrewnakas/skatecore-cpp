// Port of skate-core/src/physics/world_contact/box_feature.rs.
#include "s3/core/physics/world_contact/box_feature.hpp"

#include "s3/core/physics/reciprocal_sqrt.hpp"
#include "s3/core/physics/world_contact/feature.hpp"

namespace s3::physics::world_contact::box_feature {

namespace {
using V = std::array<float, 4>;
using feature::initialize_feature_segment;
using reciprocal_sqrt::estimate;

constexpr std::size_t OTHER[3][2] = {{1, 2}, {0, 2}, {0, 1}};  // 83044248
constexpr float SIGNS[4][2] = {{1.0f, 1.0f}, {1.0f, -1.0f}, {-1.0f, -1.0f}, {-1.0f, 1.0f}};  // 83044260

V load(const std::uint32_t* words, std::size_t offset) {
    return V{rs::from_bits(words[offset]), rs::from_bits(words[offset + 1]), rs::from_bits(words[offset + 2]),
             rs::from_bits(words[offset + 3])};
}
void store(std::uint32_t* words, std::size_t offset, V v) {
    for (std::size_t i = 0; i < 4; ++i) words[offset + i] = rs::to_bits(v[i]);
}
float dot(V a, V b) { return arithmetic::dot({a[0], a[1], a[2]}, {b[0], b[1], b[2]}); }
V scale(V a, float b) { return V{a[0] * b, a[1] * b, a[2] * b, a[3] * b}; }
V sub(V a, V b) { return V{a[0] - b[0], a[1] - b[1], a[2] - b[2], a[3] - b[3]}; }
V add(V a, V b) { return V{a[0] + b[0], a[1] + b[1], a[2] + b[2], a[3] + b[3]}; }
V madd(V a, float b, V c) {
    return V{rs::mul_add(a[0], b, c[0]), rs::mul_add(a[1], b, c[1]), rs::mul_add(a[2], b, c[2]),
             rs::mul_add(a[3], b, c[3])};
}
V neg(V a) { return V{-a[0], -a[1], -a[2], -a[3]}; }
V cross(V a, V b) {
    return V{
        rs::mul_add(-a[2], b[1], a[1] * b[2]),
        rs::mul_add(-a[0], b[2], a[2] * b[0]),
        rs::mul_add(-a[1], b[0], a[0] * b[1]),
        rs::mul_add(-a[3], b[3], a[3] * b[3]),
    };
}

V plane(V axis, V direction, std::uint32_t mode) {
    V result = mode != 0 ? cross(axis, direction) : cross(direction, axis);
    const float squared = dot(result, result);
    if (squared > rs::from_bits(0x34000000u)) {
        float r = estimate(squared);
        for (int i = 0; i < 2; ++i) {
            r = rs::mul_add(r * 0.5f, rs::mul_add(-squared, r * r, 1.0f), r);
        }
        result = scale(result, r);
    }
    return result;
}

void face(V center, const std::array<V, 3>& axes, const std::array<float, 3>& half, std::size_t fixed,
          std::uint32_t mode, V direction, MaximumFeature& output) {
    const float sign = dot(axes[fixed], direction) > 0.0f ? 1.0f : -1.0f;
    const V face_center = madd(axes[fixed], half[fixed] * sign, center);
    const float winding_sign = fixed == 1 ? -sign : sign;
    const std::size_t u = OTHER[fixed][0], v = OTHER[fixed][1];
    const V positive_u = scale(axes[u], half[u]);
    const V negative_u = scale(axes[u], -half[u]);
    const V positive_v = scale(axes[v], half[v]);
    const V negative_v = scale(axes[v], -half[v]);
    const V p = plane(axes[v], direction, mode);
    const V q = plane(axes[u], direction, mode);
    const float len_u = half[u] * 2.0f;
    const float len_v = half[v] * 2.0f;
    const std::array<V, 4> points = {
        add(add(face_center, positive_u), positive_v),
        add(add(face_center, positive_u), negative_v),
        add(add(face_center, negative_u), negative_v),
        add(add(face_center, negative_u), positive_v),
    };
    const V negative_axis_u = neg(axes[u]);
    const V negative_axis_v = neg(axes[v]);
    std::array<std::size_t, 4> indices;
    std::array<V, 4> directions;
    std::array<V, 4> planes;
    std::array<float, 4> lengths;
    if (std::uint32_t(winding_sign < 0.0f) == mode) {
        indices = {0, 1, 2, 3};
        directions = {negative_axis_v, negative_axis_u, axes[v], axes[u]};
        planes = {neg(p), neg(q), p, q};
        lengths = {len_v, len_u, len_v, len_u};
    } else {
        indices = {0, 3, 2, 1};
        directions = {negative_axis_u, negative_axis_v, axes[u], axes[v]};
        planes = {neg(q), neg(p), q, p};
        lengths = {len_u, len_v, len_u, len_v};
    }
    for (std::size_t i = 0; i < 4; ++i) {
        const std::size_t offset = 4 + i * 16;
        store(output.data(), offset, points[indices[i]]);
        store(output.data(), offset + 4, directions[i]);
        store(output.data(), offset + 8, planes[i]);
        for (std::size_t k = offset + 12; k < offset + 16; ++k) output[k] = rs::to_bits(lengths[i]);
    }
    store(output.data(), 132, scale(axes[fixed], -winding_sign));
    output[140] = 4;
}
}  // namespace

void box_maximum_feature(const std::array<std::uint32_t, 48>& gp, std::uint32_t mode,
                         std::array<std::uint32_t, 4> direction_bits, MaximumFeature& output,
                         std::array<std::uint32_t, 4> incoming_edge_plane) {
    const V center = load(gp.data(), 0);
    const std::array<V, 3> axes = {load(gp.data(), 4), load(gp.data(), 8), load(gp.data(), 12)};
    const std::array<float, 3> half = {rs::from_bits(gp[28]), rs::from_bits(gp[29]), rs::from_bits(gp[30])};
    const V direction = rs::from_bits(direction_bits);
    int small = 0;
    std::size_t small_axis = 0;
    std::size_t large_axis = 0;
    for (std::size_t i = 0; i < 3; ++i) {
        if (rs::abs(dot(direction, axes[i])) < rs::from_bits(0x3e4ccccdu)) {
            small += 1;
            small_axis = i;
        } else {
            large_axis = i;
        }
    }
    if (small == 2) {
        face(center, axes, half, large_axis, mode, direction, output);
    } else if (small == 1) {
        const std::size_t u = OTHER[small_axis][0], v = OTHER[small_axis][1];
        V best{0.0f, 0.0f, 0.0f, 0.0f};
        float score = 0.0f;
        for (std::size_t i = 0; i < 4; ++i) {
            const V point = madd(axes[v], half[v] * SIGNS[i][1], madd(axes[u], half[u] * SIGNS[i][0], center));
            const float projected = dot(point, direction);
            if (i == 0 || projected > score) {
                best = point;
                score = projected;
            }
        }
        const V negative = sub(best, scale(axes[small_axis], half[small_axis]));
        const V positive = madd(axes[small_axis], half[small_axis], best);
        std::array<std::uint32_t, 16> segment{};
        for (std::size_t i = 0; i < 4; ++i) segment[8 + i] = incoming_edge_plane[i];
        initialize_feature_segment(segment, rs::to_bits(negative), rs::to_bits(positive));
        for (std::size_t i = 0; i < 16; ++i) output[4 + i] = segment[i];
        output[140] = 1;
    } else {
        V best{0.0f, 0.0f, 0.0f, 0.0f};
        float score = 0.0f;
        for (std::size_t i = 0; i < 8; ++i) {
            const float sign = i > 3 ? 1.0f : -1.0f;
            const float* pair = SIGNS[i % 4];
            const V point = madd(axes[2], half[2] * pair[1],
                                 madd(axes[1], half[1] * pair[0], madd(axes[0], half[0] * sign, center)));
            const float projected = dot(point, direction);
            if (i == 0 || projected > score) {
                best = point;
                score = projected;
            }
        }
        store(output.data(), 136, best);
        output[140] = 0;
    }
    output[0] = 0;
}

}  // namespace s3::physics::world_contact::box_feature
