// Port of skate-game/src/physics/player_input/output_reset.rs.
//! Represented PhysOut reset values. Original82DE53F0 copies the templates
//! built by82DE4940; scalar zero does not replace any active producer.
#pragma once
#include "s3/core/player/input_phase.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::player_input::output_reset {

inline constexpr sc::player::input_phase::RawVector UP = {0, 0x3f800000u, 0, 0};

void reset_outputs(sc::player::input_phase::PhysicalPlayerInput& out);

}  // namespace s3::game::physics::player_input::output_reset
