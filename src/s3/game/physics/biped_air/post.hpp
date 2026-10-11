// Port of skate-game/src/physics/biped_air/post.rs
//! State501 Post82D2FFF8, after completed physical skeleton feedback.
#pragma once
#include "s3/core/player/offboard/biped_air/post.hpp"
#include "s3/core/player/wipeout/data.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/fwd.hpp"

namespace s3::game::physics::biped_air::post {

using ::s3::player::offboard::biped_air::recovered::post::PostInput;
using ::s3::player::wipeout::Frame;

Status post(SkaterRuntime& skater, Frame frame);

}  // namespace s3::game::physics::biped_air::post
