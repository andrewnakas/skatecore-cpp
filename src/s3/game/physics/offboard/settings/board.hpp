// Port of skate-game/src/physics/offboard/settings/board.rs
//! Ground ctor82D305D8 passes state36 to82B97060, whose full64 class/key
//! are492026E0F096F283/D7EDBD362D7D2152 (physics_state_offboard/default).
//!82B6CC78 stores the returned collection layout at wrapper4 = state40.
#pragma once
#include <string>

#include "s3/core/player/offboard/ground_sync.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::offboard::settings::board {

using BoardSettings = sc::player::offboard::ground_sync::BoardSettings;

/// pub(super)
rs::Result<BoardSettings, std::string> load(const sd::Collections& data);

}  // namespace s3::game::physics::offboard::settings::board
