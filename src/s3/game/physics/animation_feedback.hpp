// Port of skate-game/src/physics/animation_feedback.rs.
//! Persistent physical-to-animation conditioning with stock collection values.
#pragma once
#include <array>
#include <string>

#include "s3/core/animation/ground_acceleration.hpp"
#include "s3/core/animation/physical_feedback.hpp"
#include "s3/core/input/turn_conditioner.hpp"
#include "s3/core/physics/board_motion_output.hpp"
#include "s3/core/riding/pumping/state.hpp"
#include "s3/core/riding/speed_wobble.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::animation_feedback {

struct AnimationFeedback {
    // Private in Rust.
    sc::input::turn_conditioner::Settings settings{};
    sc::animation::ground_acceleration::Settings bump_settings{};
    sc::input::turn_conditioner::State state{};
    std::array<float, 4> previous_lateral_tilt{};
    // Public in Rust.
    std::array<float, 4> published_previous_lateral_tilt{};

    static rs::Result<AnimationFeedback, std::string> load(const sd::Collections& data);

    void reset();

    /// Call once after the native physical outputs are published. Absence of a
    /// completed Reckoning frame belongs to the caller, not a fallback here.
    sc::animation::physical_feedback::PhysicalFeedback update(
        const sc::physics::board_motion_output::BoardMotionOutput& motion,
        const sc::riding::pumping::state::PumpingState& pumping,
        const sc::riding::speed_wobble::SpeedWobbleState& wobble,
        sc::animation::physical_feedback::ReckoningFeedback reckoning,
        sc::animation::physical_feedback::ControlFeedback controls,
        sc::animation::ground_acceleration::Input acceleration, std::array<float, 4> lateral_tilt);
};

}  // namespace s3::game::physics::animation_feedback
