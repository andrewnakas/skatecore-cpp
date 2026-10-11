// Port of skate-game/src/physics/offboard/landing_deck/input.rs
//! Actual completed PlayerInput/BoardToolkit fields read by82D78xxx.
#pragma once
#include <string>

#include "s3/game/physics/offboard/landing_deck.hpp"

namespace s3::game::physics::offboard::landing_deck::input {

/// pub(super)
rs::Result<native::Input, std::string> processed(const PlayerInputRuntime& input);
/// pub(super)
Vector position(const PlayerInputRuntime& input);
/// pub(super)
Vector velocity(const PlayerInputRuntime& input);

}  // namespace s3::game::physics::offboard::landing_deck::input
