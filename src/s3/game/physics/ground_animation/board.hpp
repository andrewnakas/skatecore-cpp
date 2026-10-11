// Port of skate-game/src/physics/ground_animation/board.rs.
//!82D33E30: jump launch or ordered foot/brake/friction/collision forces.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/math.hpp"
#include "s3/core/physics/force_queue.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/fwd.hpp"

namespace s3::game::physics::ground_animation::board {

/// `pub(super)`.
void set_drag(GamePhysics& physics, float drag);
/// `pub(super)`.
Status advance(GamePhysics& physics, SkaterRuntime& skater);

/// Private helpers.
sc::physics::force_queue::QueuedPointForce point_force(std::uint32_t tag, std::array<float, 8> v);
sc::math::Vector3 xyz(std::array<float, 4> v);

}  // namespace s3::game::physics::ground_animation::board
