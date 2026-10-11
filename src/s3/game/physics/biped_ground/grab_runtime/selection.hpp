// Port of skate-game/src/physics/biped_ground/grab_runtime/selection.rs
//! Ground mode0 consumer of the original core82D4D150 implementation.
#pragma once
#include <array>
#include <optional>
#include <vector>

#include "s3/core/player/offboard/grab_scene.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::biped_ground::grab_runtime::selection {

using ::s3::player::offboard::grab_scene::Record;

std::optional<Record> best(const std::vector<Record>& records, std::array<float, 4> position);

}  // namespace s3::game::physics::biped_ground::grab_runtime::selection
