// Port of skate-core/src/physics/world_contact/triangle_feature.rs.
#include "s3/core/physics/world_contact/triangle_feature.hpp"

#include <optional>

#include "s3/core/physics/reciprocal_sqrt.hpp"

namespace s3::physics::world_contact::triangle_feature {

namespace {
using V = std::array<float, 4>;
using reciprocal_sqrt::estimate;

V load(const std::uint32_t* words, std::size_t offset) {
    return V{rs::from_bits(words[offset]), rs::from_bits(words[offset + 1]), rs::from_bits(words[offset + 2]),
             rs::from_bits(words[offset + 3])};
}
void store(std::uint32_t* words, std::size_t offset, V v) {
    for (std::size_t i = 0; i < 4; ++i) words[offset + i] = rs::to_bits(v[i]);
}
float dot(V a, V b) { return arithmetic::dot({a[0], a[1], a[2]}, {b[0], b[1], b[2]}); }
V cross(V a, V b) {
    return V{
        rs::mul_add(-a[2], b[1], a[1] * b[2]),
        rs::mul_add(-a[0], b[2], a[2] * b[0]),
        rs::mul_add(-a[1], b[0], a[0] * b[1]),
        rs::mul_add(-a[3], b[3], a[3] * b[3]),
    };
}
void segment(MaximumFeature& out, std::size_t index, V point, V edge, std::uint32_t length) {
    const std::size_t offset = 4 + index * 16;
    store(out.data(), offset, point);
    store(out.data(), offset + 4, edge);
    for (std::size_t k = offset + 12; k < offset + 16; ++k) out[k] = length;
}
}  // namespace

void triangle_maximum_feature(const std::array<std::uint32_t, 48>& gp, std::uint32_t mode,
                              std::array<std::uint32_t, 4> direction, MaximumFeature& out) {
    const V normal = load(gp.data(), 4);
    const V query = rs::from_bits(direction);
    const float projected = dot(query, normal);
    const std::array<V, 3> points = {load(gp.data(), 0), load(gp.data(), 8), load(gp.data(), 12)};
    const std::array<V, 3> edges = {load(gp.data(), 16), load(gp.data(), 20), load(gp.data(), 24)};
    if (rs::abs(projected) > rs::from_bits(0x3f733333u)) {
        if (std::uint32_t(projected < 0.0f) == mode) {
            const std::size_t order[3] = {0, 2, 1};
            for (std::size_t i = 0; i < 3; ++i) {
                segment(out, i, points[order[i]], edges[i], gp[28 + i]);
            }
            out[0] = 8;
        } else {
            for (std::size_t i = 0; i < 3; ++i) {
                const V e = edges[2 - i];
                segment(out, i, points[i], V{-e[0], -e[1], -e[2], -e[3]}, gp[30 - i]);
            }
            out[0] = 0;
        }
        store(out.data(), 132, normal);
        out[140] = 3;
    } else {
        const float p[3] = {dot(query, points[0]), dot(query, points[1]), dot(query, points[2])};
        const float e[3] = {rs::abs(dot(query, edges[0])), rs::abs(dot(query, edges[1])),
                            rs::abs(dot(query, edges[2]))};
        const float threshold = rs::from_bits(0x3d4ccccdu);
        std::size_t point;
        std::optional<std::size_t> edge;
        if (p[0] > p[1] && p[0] > p[2]) {
            point = 0;
            if (threshold > e[0] && e[2] > e[0]) {
                edge = 0;
            } else if (threshold > e[2]) {
                edge = 2;
            }
        } else if (p[1] > p[2]) {
            point = 1;
            if (threshold > e[1] && e[2] > e[1]) {
                edge = 1;
            } else if (threshold > e[2]) {
                edge = 2;
            }
        } else {
            point = 2;
            if (threshold > e[0] && e[1] > e[0]) {
                edge = 0;
            } else if (threshold > e[1]) {
                edge = 1;
            }
        }
        if (edge) {
            const std::size_t starts[3] = {0, 2, 1};
            segment(out, 0, points[starts[*edge]], edges[*edge], gp[28 + *edge]);
            out[140] = 1;
        } else {
            store(out.data(), 136, points[point]);
            out[140] = 0;
        }
    }
    build_feature_edge_planes(out, mode, direction);
}

void build_feature_edge_planes(MaximumFeature& feature, std::uint32_t mode, std::array<std::uint32_t, 4> direction) {
    const std::int32_t count = static_cast<std::int32_t>(feature[140]);
    if (count <= 0) {
        return;
    }
    if (!(count <= 8)) S3_PANIC("native feature storage holds at most eight segments");
    V normal = rs::from_bits(direction);
    for (float& x : normal) x = mode == 0 ? -x : x;
    for (std::size_t i = 0; i < static_cast<std::size_t>(count); ++i) {
        const std::size_t offset = 4 + i * 16;
        const V edge = load(feature.data(), offset + 4);
        const V plane = cross(edge, normal);
        const float squared = dot(plane, plane);
        float r = estimate(squared);
        r = rs::mul_add(r * 0.5f, rs::mul_add(-squared, r * r, 1.0f), r);
        const float scale = squared > rs::from_bits(0x34000000u) ? r : 0.0f;
        store(feature.data(), offset + 8, V{plane[0] * scale, plane[1] * scale, plane[2] * scale, plane[3] * scale});
    }
}

}  // namespace s3::physics::world_contact::triangle_feature
