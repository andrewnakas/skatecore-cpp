// Port of skate-game/src/physics/render_pose.rs.
//! Finish the actual physical pose and publish it to the single render skin.
//! Call after collision/error/COM output and before the next animation update.
#pragma once
#include <array>

#include "s3/game/common.hpp"
#include "s3/game/physics/fwd.hpp"

namespace s3::game::physics::render_pose {

/// pub(super)
Status publish(const GamePhysics& physics, SkaterRuntime& skater, std::array<float, 2> average_compression);

}  // namespace s3::game::physics::render_pose
