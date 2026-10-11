// Port of skate-game/src/physics/solve.rs.
//! One physical solve for the board, skater and original animation targets.
//! The caller publishes forces and drive targets before entering this phase.
#pragma once
#include <array>

#include "s3/core/physics/board_runtime.hpp"
#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/fwd.hpp"
#include "s3/game/physics/solve/assembly_contacts.hpp"
#include "s3/game/physics/solve/diagnostics.hpp"

namespace s3::game::physics::solve {

Status advance(GamePhysics& physics, SkaterRuntime& skater, std::array<float, 2> truck_targets);

sc::physics::skeleton_animation_record::AnimationPartTransform deck_frame(
    const sc::physics::board_runtime::BoardRuntime& board);

}  // namespace s3::game::physics::solve
