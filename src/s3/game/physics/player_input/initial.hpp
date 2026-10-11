// Port of skate-game/src/physics/player_input/initial.rs.
//! Original82DB18A0 initialization of the represented input owner. Native
//! opaque allocation flags are replaced by zero-initialized host storage.
#pragma once
#include <string>

#include "s3/core/player/input_phase.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::player_input::initial {

/// `pub(super)`.
rs::Result<sc::player::input_phase::PlayerInputState, std::string> player(const sd::Collections& data);

}  // namespace s3::game::physics::player_input::initial
