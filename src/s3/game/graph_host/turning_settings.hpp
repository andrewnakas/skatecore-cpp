// Port of skate-game/src/graph_host/turning_settings.rs.
//! SetTurning82BB3830/TurnRemap82BB4248 stock layout binding.
//! anim_carving/default's schema is752 bytes; native global slot372 selects it.
#pragma once
#include <string>

#include "s3/core/input/set_turning.hpp"
#include "s3/core/rs.hpp"
#include "s3/data/collections.hpp"

namespace s3::game::graph_host::turning_settings {

rs::Result<s3::input::set_turning::Settings, std::string> load(const s3::data::Collections& data);

}  // namespace s3::game::graph_host::turning_settings
