// Port of skate-game/src/physics/grind/post.rs. (C++ namespace `grind::post_`: the re-exported fn `grind::post` takes the plain name)
//! Common grind Post using CURRENT collision observations and request owner.
#pragma once
#include "s3/core/physics/grind_forces/post.hpp"
#include "s3/core/player/wipeout/requests.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/wipeout/observations.hpp"


namespace s3::game::physics::grind::post_ {

Status check(::s3::player::wipeout::requests::Requests& state, ::s3::physics::grind_forces::post::Settings settings,
             const wipeout::observations::Observations& observations);

}  // namespace s3::game::physics::grind::post_
