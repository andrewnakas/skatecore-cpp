// Port of skate-game/src/physics/grind/condition.rs. (C++ namespace `grind::condition_`: the re-exported fn `grind::condition` takes the plain name)
//! Completed PhysOut -> chromosome82DEE508, before filtered82DE56B0.
#pragma once
#include "s3/game/common.hpp"
#include "s3/game/physics/fwd.hpp"

namespace s3::game::physics::grind::condition_ {

/// Run on EVERY completed publication, not merely physical grind frames.
///82DF2130 installs chromosome at slot1 and filtered at slot5;82DF2710
///dispatches in ascending order. Board/Skeleton/state Fill must precede this.
Status condition(const GamePhysics& physics, SkaterRuntime& skater);

}  // namespace s3::game::physics::grind::condition_
