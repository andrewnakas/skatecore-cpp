// Port of skate-core/src/player/offboard/ground_entry.rs (+ math.rs).
// Numerical BipedGround Enter82D30808 and local reset fields82D30BD0.
// The game lifecycle also resets toolkit/contact data, initializes the controller,
// enters board manager and skeleton mode4, and resets trajectory helpers in order.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/player/offboard/ground_entry/math.hpp"

namespace s3::player::offboard::ground_entry {

using Vector = std::array<float, 4>;
using Frame = std::array<Vector, 4>;

struct Input {
    /// Actual82BE3650 animation-to-world frame, before changing skeleton mode.
    Frame animation_frame{};
    Vector processed_velocity_608{};
    std::uint32_t processed_flags_2484 = 0;
    std::uint32_t processed_flags_2476 = 0;
    float requested_angle_2936 = 0.0f;
    float requested_duration_2896 = 0.0f;
    std::uint32_t previous_state_2504 = 0;
    Vector previous_frame_up_208{};
    /// Actual skeleton15872; feeds Biped initialization input80.
    Vector body_position_15872{};
};

struct Placement {
    Frame frame{};
    Vector planar_velocity{};
    Vector body_position{};
};


/// Ground physical-state fields, separate from the Biped controller.
struct State {
    Frame frame_80 = math::IDENTITY;
    std::array<bool, 7> flags_144_to_150{};
    std::uint32_t counter_152 = 0;
    std::uint32_t counter_156 = 0;
    float elapsed_160 = 0.0f;
    float distance_164 = rs::from_bits(0x501502f9u);
    float elapsed_168 = 0.0f;
    float angle_172 = 0.0f;
    float angular_velocity_176 = 0.0f;
    float duration_180 = 0.0f;

    /// Call following toolkit/contact reset; pass returned fields to82D7B7A0.
    Placement enter(const Input& input);
};


}  // namespace s3::player::offboard::ground_entry
