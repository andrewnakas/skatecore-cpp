// Port of skate-core/src/physics/grind_forces.rs.
#include "s3/core/physics/grind_forces.hpp"

namespace s3::physics::grind_forces {

std::optional<V> lateral_pin(std::array<V, 4> board, V point, V across, V velocity, float strength,
                             float forward_offset, float up_offset, bool forward_selected,
                             float slope_multiplier) {
    const V direction = forward_selected ? board[2] : scale(board[2], -1.0f);
    V reference{};
    for (std::size_t i = 0; i < 4; ++i) {
        reference[i] = rs::mul_add(direction[i], forward_offset, board[3][i]) - board[1][i] * up_offset;
    }
    const V force = scale(across, dot3(sub(point, reference), across) * strength);
    const float length_ = length(force);
    if (!(length_ > 0.0f)) {
        return std::nullopt;
    }
    const V axis = scale(force, reciprocal(length_));
    const V damping = scale(axis, strength * rs::from_bits(0x3e083127u) * dot3(velocity, axis));
    return scale(sub(force, damping), slope_multiplier);
}

V friction(V velocity, V grind_normal, V support, float time_multiplier, bool flagged_surface,
           float surface_multiplier, std::uint32_t geometry_kind, std::array<float, 3> strengths) {
    const V tangent_velocity = sub(velocity, scale(grind_normal, dot3(velocity, grind_normal)));
    const float speed = length(tangent_velocity);
    if (speed <= 0.001f) {
        return V{0.0f, 0.0f, 0.0f, 0.0f};
    }
    const float strength = strengths[geometry_kind == 0 ? 0 : geometry_kind == 1 ? 1 : 2];
    const float load = rs::max(support[1], 0.0f);
    const float surface_flag_multiplier = flagged_surface ? 1.9f : 1.0f;
    const float multiplier = load / speed * time_multiplier * surface_flag_multiplier *
                             surface_multiplier * strength * rs::from_bits(0xbef5c28fu);
    return scale(tangent_velocity, multiplier);
}

bool friction_applies(V velocity, V normal) {
    const V planar = sub(velocity, scale(normal, dot3(velocity, normal)));
    return length(planar) > 0.001f;
}

float material_multiplier(std::uint32_t material) {
    switch (material) {
        case 2:
        case 13:
            return 1.7f;
        case 3:
            return 2.6f;
        case 4:
            return 0.6f;
        case 5:
            return 4.0f;
        case 6:
            return 5.0f;
        default:
            return 1.3f;
    }
}

float pin_slope(V normal, V upmost_normal, float gravity_relief,
                const point_graph::PointGraph<4>& pin_vs_slope) {
    if (gravity_relief > 0.0f) {
        return 1.0f;
    }
    return pin_vs_slope.evaluate(dot3(normal, upmost_normal));
}

}  // namespace s3::physics::grind_forces
