// Port of skate-game/src/graph_host/motion_manual.rs (module `motion_manual`).
//! Stock manual animation data. Physical controller state remains in Ground.
#pragma once
#include <string>

#include "s3/core/animation/manual.hpp"
#include "s3/core/rs.hpp"
#include "s3/data/collections.hpp"

namespace s3::game::graph_host::motion_manual {

rs::Result<s3::animation::manual::Settings, std::string> settings(const s3::data::Collections& data);

}  // namespace s3::game::graph_host::motion_manual
