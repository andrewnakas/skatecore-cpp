// Port of skate-game/src/graph_host/crouching_settings.rs.
//! anim_motion/crouching (global300) and auto_pump (global348), TU3 schema.
#pragma once
#include <string>

#include "s3/core/animation/crouching.hpp"
#include "s3/core/rs.hpp"
#include "s3/data/collections.hpp"

namespace s3::game::graph_host::crouching_settings {

rs::Result<s3::animation::crouching::Settings, std::string> load(const s3::data::Collections& data);

}  // namespace s3::game::graph_host::crouching_settings
