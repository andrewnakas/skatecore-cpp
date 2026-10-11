// Port of skate-core/src/player/offboard/board_possession/lifecycle.rs.
// The types and State methods live in board_possession.hpp/.cpp (one State owner);
// this header re-exports them under the Rust module path.
#pragma once
#include "s3/core/player/offboard/board_possession.hpp"

namespace s3::player::offboard::board_possession::lifecycle {
using board_possession::Alignment;
using board_possession::Effects;
using board_possession::Observation;
using board_possession::Settings;
}  // namespace s3::player::offboard::board_possession::lifecycle
