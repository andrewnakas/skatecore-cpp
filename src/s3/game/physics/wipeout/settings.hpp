// Port of skate-game/src/physics/wipeout/settings.rs
//! Globals264 -> original8289D270 class EB844CFDBF928EFD/default.
//! Fields are independently mapped through the stock physics_wipeout schema.
#pragma once
#include <array>
#include <string>
#include <utility>

#include "s3/core/player/wipeout.hpp"
#include "s3/core/point_graph.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::wipeout::settings {

rs::Result<std::pair<::s3::player::wipeout::Settings, std::array<::s3::player::wipeout::Mode, 5>>, std::string> load(
    const sd::Collections& data);

}  // namespace s3::game::physics::wipeout::settings
