// Port of skate-core/src/riding/speed_model.rs.
// TU3 Skateboard::UpdateSpeedModel 82C04F68, including its force-enable gate.
// The caller supplies GetEffectiveTransform and GetMass outputs, not a guessed
// direction or mass. Ground submits this result as force tag 6.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/point_graph.hpp"

namespace s3::riding::speed_model {

using point_graph::PointGraph;

struct SpeedModelSettings {
    /// physics_speedmodel layout +0,+8,+16,+20,+24.
    float negative_gain = 0.0f;
    float maximum_gravity_acceleration = 0.0f;
    float gravity = 0.0f;
    float positive_gain = 0.0f;
    float coffin_acceleration = 0.0f;
    /// Surface layout +260 and curve +96/+128.
    float speed_error_bound = 0.0f;
    PointGraph<8> surface_friction{};
    /// physics_manual layout +180,+212,+220.
    float manual_acceleration = 0.0f;
    float negative_manual_angle_limit = 0.0f;
    float manual_angle_limit = 0.0f;
    /// Global +184 layout +356 and curves +144/+176,+224/+256.
    float no_input_delay = 0.0f;
    PointGraph<8> no_input_friction{};
    PointGraph<8> manual_friction{};
    /// Selected-mode layout +44 and +40.
    bool override_enabled = false;
    float override_speed = 0.0f;
    /// Live constants 830BD350 and 830BD380 (four native lanes each).
    std::array<float, 4> normal_threshold{};
    std::array<float, 4> override_direction_threshold{};
};

struct SpeedModelState {
    float target_speed = 0.0f;  // Board +260
    std::uint32_t flags_1360 = 0;
};

struct SpeedModelInput {
    std::uint32_t flags_2468 = 0;
    std::uint32_t flags_2476 = 0;
    std::int32_t wheel_contact_count = 0;  // +2556
    std::int32_t frames_2580 = 0;
    float timestep = 0.0f;      // +2604
    float signed_speed = 0.0f;  // +2612
    float angle_2652 = 0.0f;
    float surface_speed = 0.0f;  // +2656
    float turn_2672 = 0.0f;
    float balance = 0.0f;                // +2720
    float elapsed_without_input = 0.0f;  // +2748
    float manual_state_276 = 0.0f;       // Board +276
    std::array<float, 4> vector_160{};
    std::array<float, 4> vector_352{};
    std::array<float, 4> velocity_416{};
    std::array<float, 4> normal_464{};
    /// Z column of 82C01BF8's result; stance correction has already happened.
    std::array<float, 4> effective_forward{};
    /// 82C06F58 result, whose two-accumulator summation is implemented in mass.
    float mass = 0.0f;
};

/// Force vector followed by zeroed local application point, all four lanes.
std::array<float, 8> update(SpeedModelState& state, const SpeedModelSettings& settings, const SpeedModelInput& input);

}  // namespace s3::riding::speed_model
