// Port of skate-game/src/physics/biped_air/settings.rs
//! Stock physics_wipeout fields read by82D8FEE8/82D90000 only.
#pragma once
#include <string>

#include "s3/core/player/offboard/biped_air/post.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::biped_air::settings {

using ::s3::player::offboard::biped_air::recovered::post::Settings;
using ::s3::player::offboard::biped_air::recovered::post::checks::Thresholds;

rs::Result<Settings, std::string> load(const sd::Collections& data);

}  // namespace s3::game::physics::biped_air::settings
