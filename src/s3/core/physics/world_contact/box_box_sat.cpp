// Port of skate-core/src/physics/world_contact/box_box_sat.rs.
#include "s3/core/physics/world_contact/box_box_sat.hpp"

#include "s3/core/physics/reciprocal_sqrt.hpp"
#include "s3/core/physics/world_contact/prism_math.hpp"

namespace s3::physics::world_contact::primitive_query::box_box_sat {

using namespace prism_math;
using reciprocal_sqrt::estimate;

std::pair<std::array<std::uint32_t, 4>, std::array<std::uint32_t, 4>> box_box(const std::array<std::uint32_t, 48>& a,
                                                                              const std::array<std::uint32_t, 48>& b) {
    std::array<V, 16> axes{};
    for (std::size_t i = 0; i < 3; ++i) {
        axes[i] = load(a, 12 - i * 4);
        axes[3 + i] = load(b, 12 - i * 4);
    }
    for (std::size_t edge_a = 0; edge_a < 3; ++edge_a) {
        for (std::size_t edge_b = 0; edge_b < 3; ++edge_b) {
            const V axis = cross(load(a, 16 + edge_a * 4), load(b, 16 + edge_b * 4));
            axes[6 + edge_a * 3 + edge_b] = scale(axis, estimate(dot(axis, axis)));
        }
    }
    axes[15] = axes[14];
    std::array<V, 3> scaled_a, scaled_b;
    for (std::size_t i = 0; i < 3; ++i) {
        scaled_a[i] = scale(load(a, 4 + i * 4), rs::from_bits(a[28 + i]));
        scaled_b[i] = scale(load(b, 4 + i * 4), rs::from_bits(b[28 + i]));
    }
    float best = 0.0f;
    V selected = axes[0];
    float selected_sign = 1.0f;
    for (std::size_t index = 0; index < 16; ++index) {
        const V axis = axes[index];
        const float a_center = dot(load(a, 0), axis);
        const float b_center = dot(load(b, 0), axis);
        float a_extents[3], b_extents[3];
        for (std::size_t i = 0; i < 3; ++i) {
            a_extents[i] = rs::abs(dot(scaled_a[i], axis));
            b_extents[i] = rs::abs(dot(scaled_b[i], axis));
        }
        const float a_radius = (a_extents[0] + a_extents[1]) + a_extents[2];
        const float b_radius = (b_extents[0] + b_extents[1]) + b_extents[2];
        const float forward = (a_center - a_radius) - (b_center + b_radius);
        const float reverse = (b_center - b_radius) - (a_center + a_radius);
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

}  // namespace s3::physics::world_contact::primitive_query::box_box_sat
