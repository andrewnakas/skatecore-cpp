// Port of skate-core/src/physics/grind_forces/support.rs.
// Common force leaves82D40080/82D3FFC0/82D40130/82D401F8.
// These consume manager output; they do not duplicate its assistance update.
#pragma once
#include "s3/core/physics/grind_forces.hpp"

namespace s3::physics::grind_forces::support {

inline std::optional<V> pin(V normal, float gravity_relief) {
    if (!(gravity_relief > 0.0f)) {
        return std::nullopt;
    }
    V force = scale(normal, -40.0f);
    force[1] = 0.0f;
    return force;
}

inline std::optional<V> coping(V upmost, V velocity, float gravity_relief) {
    if (gravity_relief > 0.0f && dot3(upmost, velocity) < 0.0f) {
        return V{0.0f, 120.0f, 0.0f, 0.0f};
    }
    return std::nullopt;
}

inline std::optional<V> exit_lean(V high_side, float angle_1500, const point_graph::PointGraph<4>& graph) {
    if (angle_1500 > 0.0f) {
        return scale(high_side, graph.evaluate(angle_1500) * -65.0f);
    }
    return std::nullopt;
}

/// Unlike conditional pin/coping, original always calls AddForce, even at0.
inline V truck_compensation(V normal, float pitch_1508) {
    return scale(normal, rs::abs(pitch_1508) * -600.0f);
}

}  // namespace s3::physics::grind_forces::support
