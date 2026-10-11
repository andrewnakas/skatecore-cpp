// Port of skate-game/src/physics/player_input/reset.rs.
//! Selective reset of the represented ProcessedPhysIn fields, TU3 82BF9EF0.
//! Packet buffers and retained flag lanes deliberately survive this reset.
#pragma once
#include "s3/core/player/input_phase.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::player_input::reset {

void reset_processed(sc::player::input_phase::ProcessedPhysicsInput& input);

}  // namespace s3::game::physics::player_input::reset
