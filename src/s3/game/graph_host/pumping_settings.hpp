// Port of skate-game/src/graph_host/pumping_settings.rs.
//! Stock anim_motion/anim_pump; TU3 global308 layout offsets1152/1708..1784.
#pragma once
#include <string>

#include "s3/core/animation/pumping_channel.hpp"
#include "s3/core/rs.hpp"
#include "s3/data/collections.hpp"

namespace s3::game::graph_host::pumping_settings {

rs::Result<s3::animation::pumping_channel::Settings, std::string> load(const s3::data::Collections& data);

}  // namespace s3::game::graph_host::pumping_settings
