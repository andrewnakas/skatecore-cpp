// Port of skate-core/src/physics/board_dynamic_normal.rs.
// Dynamic wheel-response normal, original TU3 82C02388, with acceleration
// history from82C082AC..8348. This is Ground64, distinct from wheel normals.
#pragma once
#include <utility>

#include "s3/core/math.hpp"
#include "s3/core/physics/board_ground.hpp"
#include "s3/core/point_graph.hpp"

namespace s3::physics::board_dynamic_normal {

using board_ground::BoardGroundState;
using math::Vector3;
using point_graph::PointGraph;

struct DynamicNormalSettings {
    float speed_damping = 0.0f;
    float up_vector_damping = 0.0f;
    float maximum_delta = 0.0f;
    float speed_scale = 0.0f;
    PointGraph<8> maximum_delta_vs_speed;
};

struct BoardDynamicNormal {
    /// Skateboard112, published to Ground64.
    Vector3 normal;
    /// Skateboard128. The constructor really initializes this to UP.
    Vector3 delta;
    /// Skateboard144 and160, retained independently by the source.
    Vector3 acceleration;
    Vector3 last_contact_normal;

    /// 82C00ED0 initializes112/128 to UP and144/160 to zero. The full board
    /// reset82C0D680 ->82C00D68 clears all seven previous velocities.
    static BoardDynamicNormal new_();
    void update(const BoardGroundState& contacts, Vector3 gravity, float previous_ground_speed,
                const DynamicNormalSettings& settings);
};

/// Rust-private helpers, exposed for completeness.
Vector3 normalize(Vector3 v);
std::pair<Vector3, float> normalize_length(Vector3 v);

}  // namespace s3::physics::board_dynamic_normal
