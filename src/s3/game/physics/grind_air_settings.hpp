// Port of skate-game/src/physics/grind_air_settings.rs.
//! Stock GrindAirAdjust settings82D72B40; lengths from the actual deck/truck collections.
#pragma once
#include <string>

#include "s3/core/physics/grind_air.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::grind_air_settings {

rs::Result<::s3::physics::grind_air::Settings, std::string> load(const sd::Collections& data);

}  // namespace s3::game::physics::grind_air_settings
