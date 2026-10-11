// Port of skate-game/src/physics/animation_feedback_settings.rs.
//! PhysOutConditioner_Animation turn settings, TU3 global204/physics_animation.
#pragma once
#include <string>

#include "s3/core/input/turn_conditioner.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::animation_feedback_settings {

rs::Result<sc::input::turn_conditioner::Settings, std::string> load(const sd::Collections& data);

}  // namespace s3::game::physics::animation_feedback_settings
