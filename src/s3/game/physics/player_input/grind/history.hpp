// Port of skate-game/src/physics/player_input/grind/history.rs.
//! (`impl GrindInputState` permission/advance_history/material_mode are declared in grind.hpp.)
#pragma once
#include <cstdint>

#include "s3/game/physics/player_input/grind.hpp"

namespace s3::game::physics::player_input::grind::history {

/// `pub(super)`.
std::uint32_t decrement(std::uint32_t value);

}  // namespace s3::game::physics::player_input::grind::history
