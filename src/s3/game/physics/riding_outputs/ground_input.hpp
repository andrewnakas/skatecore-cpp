// Port of skate-game/src/physics/riding_outputs/ground_input.rs.
//! Snapshot inputs copied by original TU3 82D8E5C0 for the Ground job.
#pragma once
#include <cstdint>

#include "s3/core/math.hpp"
#include "s3/core/player/input_phase.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::riding_outputs::ground_input {

/// pub(super)
struct GroundPacketInputs {
    sc::math::Vector3 wheel_normal{};
    sc::math::Vector3 dynamic_up{};
    float speed = 0.0f;
    float absolute_speed = 0.0f;
    std::int32_t wheel_count = 0;

    static GroundPacketInputs from_processed(const sc::player::input_phase::ProcessedPhysicsInput& input);
};

}  // namespace s3::game::physics::riding_outputs::ground_input
