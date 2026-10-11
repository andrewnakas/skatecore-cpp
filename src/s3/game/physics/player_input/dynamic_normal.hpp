// Port of skate-game/src/physics/player_input/dynamic_normal.rs.
//! Stock bindings for original82C02388. Hashes were matched to the actual
//! collection names; XML layoutC85F0-C8190 verifies curve offset1120.
#pragma once
#include <string>

#include "s3/core/physics/board_dynamic_normal.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::player_input::dynamic_normal {

rs::Result<sc::physics::board_dynamic_normal::DynamicNormalSettings, std::string> settings(
    const sd::Collections& data);

}  // namespace s3::game::physics::player_input::dynamic_normal
