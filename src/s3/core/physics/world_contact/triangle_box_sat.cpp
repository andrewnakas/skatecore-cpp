// Port of skate-core/src/physics/world_contact/triangle_box_sat.rs.
#include "s3/core/physics/world_contact/triangle_box_sat.hpp"

#include "s3/core/physics/reciprocal_sqrt.hpp"
#include "s3/core/physics/world_contact/prism_math.hpp"

namespace s3::physics::world_contact::primitive_query::triangle_box_sat {

using namespace prism_math;
using reciprocal_sqrt::estimate;

namespace {
float minimum(float a, float b) { return a < b ? a : b; }
float maximum(float a, float b) { return a > b ? a : b; }
}  // namespace

std::pair<std::array<std::uint32_t, 4>, std::array<std::uint32_t, 4>> triangle_box(
    const std::array<std::uint32_t, 48>& triangle, const std::array<std::uint32_t, 48>& box_shape) {
    std::array<V, 13> axes{};
    axes[0] = load(triangle, 4);
    for (std::size_t i = 0; i < 3; ++i) {
        axes[1 + i] = load(box_shape, 12 - i * 4);
    }
    for (std::size_t edge = 0; edge < 3; ++edge) {
        for (std::size_t box_edge = 0; box_edge < 3; ++box_edge) {
            const V axis = cross(load(triangle, 16 + edge * 4), load(box_shape, 16 + box_edge * 4));
            const float squared = dot(axis, axis);
            const float seed = estimate(squared);
            const float inverse = rs::mul_add(seed * 0.5f, rs::mul_add(-squared, seed * seed, 1.0f), seed);
            axes[4 + edge * 3 + box_edge] = scale(axis, inverse);
        }
    }
    std::array<V, 3> scaled_box;
    for (std::size_t i = 0; i < 3; ++i) {
        scaled_box[i] = scale(load(box_shape, 4 + i * 4), rs::from_bits(box_shape[28 + i]));
    }
    float best = 0.0f;
    V selected = axes[0];
    float selected_sign = 1.0f;
    for (std::size_t index = 0; index < 13; ++index) {
        const V axis = axes[index];
        const float p0 = dot(load(triangle, 0), axis);
        const float p1 = dot(load(triangle, 8), axis);
        const float p2 = dot(load(triangle, 12), axis);
        // The source compares vertices 1 and 2, then combines vertex 0.
        const float min_ = minimum(p0, minimum(p1, p2));
        const float max_ = maximum(p0, maximum(p1, p2));
        const float center = dot(load(box_shape, 0), axis);
        float extents[3];
        for (std::size_t i = 0; i < 3; ++i) extents[i] = rs::abs(dot(scaled_box[i], axis));
        const float radius = (extents[0] + extents[1]) + extents[2];
        const float forward = min_ - (center + radius);
        const float reverse = (center - radius) - max_;
        const float separation = forward > reverse ? forward : reverse;
        const float sign = forward > reverse ? -1.0f : 1.0f;
        if (index == 0 || separation > best) {
            best = separation;
            selected = axis;
            selected_sign = sign;
        }
    }
    const std::uint32_t bb = rs::to_bits(best);
    return {{bb, bb, bb, bb}, bits(scale(selected, selected_sign))};
}

}  // namespace s3::physics::world_contact::primitive_query::triangle_box_sat
