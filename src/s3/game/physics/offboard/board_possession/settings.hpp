// Port of skate-game/src/physics/offboard/board_possession/settings.rs
//! Constructor74DD8 class7DEA05DC70A1ED7D/defaultD7EDBD362D7D2152.
#pragma once
#include <string>

#include "s3/core/player/offboard/board_possession/lifecycle.hpp"
#include "s3/core/point_graph.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::offboard::board_possession::settings {

using Settings = sc::player::offboard::board_possession::lifecycle::Settings;

///091F8 full hash8D3065CED858A7DF, literal822F860C=426FFFFF.
rs::Result<float, std::string> standard_angular_drag(const sd::Collections& data);
rs::Result<Settings, std::string> load(const sd::Collections& data);
/// private
rs::Result<sc::point_graph::PointGraph<8>, std::string> curve(const sd::Collections& data, const std::string& name);

}  // namespace s3::game::physics::offboard::board_possession::settings
