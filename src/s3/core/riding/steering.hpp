// Port of skate-core/src/riding/steering.rs.
// TU3 steering tilt (`0x82D92440`) and truck targets (`0x82C040F0`).
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/point_graph.hpp"

namespace s3::riding::steering {

using point_graph::PointGraph;

struct SteeringSettings {
    float hard_turn_increase = 0.0f;
    float damping = 0.0f;
    float speed_graph_max_speed = 0.0f;
    float push_scalar_increment = 0.0f;
    float push_scalar_decrement = 0.0f;
    float push_scalar_min = 0.0f;
    float manual_scalar = 0.0f;
    float general_scalar = 0.0f;
    float tight_trucks_scalar = 0.0f;
    PointGraph<8> speed_graph{};
    PointGraph<8> input_graph{};
    float tilt_blending = 0.0f;
};

/// Field names describe the reads from `ProcessedPhysIn`. No yaw-rate or
/// bicycle steering interpretation is applied to the returned tilt.
struct SteeringInput {
    float turn = 0.0f;
    float hard_turn = 0.0f;
    float absolute_body_speed = 0.0f;
    float flipped_controls_scalar = 0.0f;
    float balance = 0.0f;
    float truck_tightness = 0.0f;
    /// `ProcessedPhysIn+2468`, bit 27 (distinct from push-force bit 25).
    bool pushing = false;
};

/// Optional pointers in the native function are represented independently.
/// Updates are per physics call; the native routine has no timestep argument.
/// (Rust `Option<&mut f32>` -> nullable pointer.)
float calculate_tilt(const SteeringSettings& settings, SteeringInput input, float* push_scalar,
                     float* damped_turn);

struct TruckSteeringState {
    float deck_tilt = 0.0f;
    /// Native `SkateboardBody+7680/+7684` order, not stance-relative labels.
    std::array<float, 2> targets{};
    std::array<float, 2> activation_time{};

    /// Flags are retained in their native words to make stance-dependent
    /// contact ownership explicit (`+2468` bit 20, `+2472` bits 26 and 27).
    void update(float target, float blend, std::uint32_t flags_2468, std::uint32_t flags_2472);
};
S3_DERIVE_EQ(TruckSteeringState, deck_tilt, targets, activation_time)

}  // namespace s3::riding::steering
