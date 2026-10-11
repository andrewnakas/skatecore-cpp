// Port of skate-game/src/physics/solve/diagnostics.rs.
//! Locate invalid physical values before they propagate into a rendered pose.
#pragma once
#include <string>
#include <vector>

#include "s3/core/physics/assembly.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/fwd.hpp"

namespace s3::game::physics::solve::diagnostics {

std::vector<sc::physics::assembly::BodySnapshot> snapshot(const GamePhysics& physics, const SkaterRuntime& skater);

Status validate(const std::vector<sc::physics::assembly::BodySnapshot>& bodies, const char* stage);

}  // namespace s3::game::physics::solve::diagnostics
