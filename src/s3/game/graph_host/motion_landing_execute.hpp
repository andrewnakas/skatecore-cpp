// Port of skate-game/src/graph_host/motion_landing_execute.rs.
//! Original landing Begin/Update/End effects, called by the production graph.
#pragma once
#include <cstdint>
#include <optional>
#include <string>

#include "s3/core/rs.hpp"
#include "s3/game/graph_host/motion_animation.hpp"
#include "s3/game/graph_host/motion_landing.hpp"
#include "s3/game/graph_host/motion_riding_conditions.hpp"

namespace s3::game::graph_host::motion_landing_execute {

using motion_animation::MotionAnimation;
using motion_landing::Flags;
using motion_landing::Operation;
using motion_landing::Physical;
using motion_landing::State;
using motion_riding_conditions::MotionRandom;

rs::Result<rs::Unit, std::string> execute(const Operation& operation, State& state, Flags& flags,
                                          MotionAnimation& animation, const MotionRandom& random,
                                          std::optional<Physical> physical, std::optional<bool> mirrored, float dt,
                                          std::uint8_t phase);

}  // namespace s3::game::graph_host::motion_landing_execute
