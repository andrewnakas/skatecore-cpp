// Port of skate-game/src/physics/skeleton_colliders.rs.
//! Bind original physical skater shapes to the bodies receiving solver reactions.
//! The collision owner selects enabled parts and materials; shapes and local-Z
//! capsule axes come from the same stock definition used to construct each part.
#pragma once
#include <string>
#include <vector>

#include "s3/core/physics/board_world.hpp"
#include "s3/core/physics/skeleton_body.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::skeleton_colliders {

rs::Result<std::vector<sc::physics::board_world::BoardWorldVolume>, std::string> world_volumes(
    const sc::physics::skeleton_body::SkeletonBody& skeleton,
    const sc::physics::skeleton_body::SkeletonCollisionMode& collision);

/// Volume+84 group4 disables world contacts only. Keep these shapes available
/// to the separate SkaterSkaterCollisionPipeline and its own pair filters.
void retain_world_volumes(std::vector<sc::physics::board_world::BoardWorldVolume>& volumes,
                          const sc::physics::skeleton_body::SkeletonCollisionMode& collision);

rs::Result<std::vector<sc::physics::board_world::BoardWorldVolume>, std::string> enabled_volumes(
    const sc::physics::skeleton_body::SkeletonBody& skeleton,
    const sc::physics::skeleton_body::SkeletonCollisionMode& collision);

}  // namespace s3::game::physics::skeleton_colliders
