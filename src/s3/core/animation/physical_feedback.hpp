// Port of skate-core/src/animation/physical_feedback.rs.
//! Physical publications consumed by riding animation, TU3 82DB6EC0.
//!
//! This receives the actual board, Pumping, SpeedWobble and Reckoning results.
//! It does not reconstruct the skater frame from the deck or rendered pose.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/animation/crouching.hpp"
#include "s3/core/animation/ground_acceleration.hpp"
#include "s3/core/input/set_turning.hpp"
#include "s3/core/input/turn_conditioner.hpp"
#include "s3/core/math.hpp"
#include "s3/core/physics/board_motion_output.hpp"
#include "s3/core/riding/pumping/state.hpp"
#include "s3/core/riding/speed_wobble.hpp"
#include "s3/core/rs.hpp"

namespace s3::animation::physical_feedback {

using math::Vector3;
using physics::board_motion_output::BoardMotionOutput;
using riding::pumping::state::PumpingState;
using riding::speed_wobble::SpeedWobbleState;

/// The completed Reckoning owner's output required by animation publication.
/// There is intentionally no default: these are distinct physical frames.
struct ReckoningFeedback {
    /// PhysOutSystemReckoning+64.
    Vector3 system_position;
    /// Reckoning+1152 -> PhysOutSystemReckoning+96 at82DB7068.
    Vector3 system_up;
    /// PhysOutDeck+48.
    Vector3 board_position;
    /// Reckoning+1576, before the Processed2468 bit20 sign correction.
    float target_lean_angle = 0.0f;
};

struct ControlFeedback {
    /// The complete ProcessedPhysIn+2468 flag word.
    std::uint32_t processed_flags = 0;
    /// ProcessedPhysIn+2676 -> PhysOutGround+264.
    float turn = 0.0f;
    /// AnimOutPhysIn+10370 -> PhysOutAnimation+156.
    bool animation_mirrored = false;
};

struct PhysicalFeedback {
    input::set_turning::Physical turning;
    crouching::Physical crouching;
    float pumping_acceleration = 0.0f;
    /// PhysOutAnimation112, conditioned actual deck acceleration82DF0508.
    std::array<float, 4> ground_acceleration{};
    /// Original IsBumped82BA7310 evaluated with anim_motion/bumps.
    bool bumped = false;
    /// Complete PhysOutAnimation32..60, retained for other native consumers.
    input::turn_conditioner::Output conditioned_turn{};
};

/// Postphysics publication followed by the animation turn conditioner82DEFF38.
/// Stateful filtering happens once per conditioner update, without a dt factor.
PhysicalFeedback publish(input::turn_conditioner::State& state, const input::turn_conditioner::Settings& settings,
                         const BoardMotionOutput& motion, const PumpingState& pumping,
                         const SpeedWobbleState& wobble, ReckoningFeedback reckoning, ControlFeedback controls,
                         ground_acceleration::Output acceleration);

}  // namespace s3::animation::physical_feedback
