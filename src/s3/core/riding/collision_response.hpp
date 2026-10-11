// Port of skate-core/src/riding/collision_response.rs (whole file).
// Complete collision response calculation, TU3 82D944E8 and its angle helpers.
#pragma once
#include <array>
#include <cstdint>
#include <optional>

#include "s3/core/math.hpp"
#include "s3/core/point_graph.hpp"

namespace s3::riding::collision_response {

using math::Vector3;
using point_graph::PointGraph;

struct CollisionResponseSettings {
    float maximum_velocity_delta = 0.0f;
    float force_y_offset = 0.0f;
    float force_scalar = 0.0f;
    float target_displacement_velocity = 0.0f;
    PointGraph<8> torque_vs_angle{};
};

struct CollisionResponsePhysical {
    std::uint32_t flags_2472 = 0;
    /// Processed736,416,448,544 and Ground1216 respectively.
    std::array<float, 4> collision_displacement{};
    std::array<float, 4> velocity{};
    std::array<float, 4> forward{};
    std::array<float, 4> up{};
    std::array<float, 4> ground_normal{};
    /// Processed2604 and2660.
    float time_step = 0.0f;
    float mass = 0.0f;
};

struct CollisionResponse {
    /// Native return value. False after the delta threshold still writes all
    /// three outputs, including the target velocity and angular correction.
    bool applied = false;
    std::array<float, 4> force{};
    std::array<float, 4> point{};
    std::array<float, 4> angular_displacement{};
    std::array<float, 4> target_velocity{};
};
S3_DERIVE_EQ(CollisionResponse, applied, force, point, angular_displacement, target_velocity)

/// None represents the two early returns which write no outputs. The native
/// late false return must remain distinguishable from them.
std::optional<CollisionResponse> collision_response(const CollisionResponseSettings& settings,
                                                    CollisionResponsePhysical input);

/// 8296EC98, one-refinement normalization and signed cross about supplied up.
float signed_angle(Vector3 a, Vector3 b, Vector3 up);

}  // namespace s3::riding::collision_response
