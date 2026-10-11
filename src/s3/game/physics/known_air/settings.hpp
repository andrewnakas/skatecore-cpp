// Port of skate-game/src/physics/known_air/settings.rs.
//! Original KnownAir82D35508/82D35F40/82D360B0/82D36650 stock reads.
#pragma once
#include <array>
#include <string>
#include <utility>

#include "s3/core/air/known.hpp"
#include "s3/core/point_graph.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::known_air::settings {

rs::Result<std::pair<::s3::air::known::KnownAirSettings, std::array<::s3::air::known::KnownAirModeSettings, 5>>,
           std::string>
load(const sd::Collections& data);

}  // namespace s3::game::physics::known_air::settings
