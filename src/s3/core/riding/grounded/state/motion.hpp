// Port of skate-core/src/riding/grounded/state/motion.rs.
// Physical Ground entry82D37604..82D378A8 and future-deck82D38630 math.
// These operate on actual board rates/force records. Skeleton publication is
// separate because prediction moves its animation target, not the board.
#pragma once
#include <array>
#include <optional>

#include "s3/core/math.hpp"
#include "s3/core/physics/force_queue.hpp"

namespace s3::riding::grounded::state::motion {

using math::Vector3;
using physics::force_queue::BoardForceQueue;
using physics::force_queue::QueuedPointForce;

/// Projection is applied directly to deck angular velocity, before speed seed.
Vector3 entry_angular_velocity(std::array<float, 4> normal, std::array<float, 4> angular);
/// Board+260 is the smaller absolute longitudinal speed before/after normal
/// rejection. The source fsel retains the raw value on an unordered compare.
float entry_target_speed(Vector3 velocity, std::array<float, 4> normal, std::array<float, 4> forward);
/// Exact tag19 producer. Gravity and normal velocity are not added here.
QueuedPointForce landing_on_deck_force(std::array<float, 4> physical_velocity, std::array<float, 4> animation_velocity,
                                       std::array<float, 4> axis, float mass, float strength, float point_y);
/// Complete displacement calculation in PredictFutureDeck. Force summation
/// preserves queue order; normal rejection occurs after the first dt/mass.
/// Returns None when the source deliberately preserves both skeleton targets.
std::optional<Vector3> future_deck_displacement(const BoardForceQueue& forces, float mass, float dt,
                                                std::array<float, 4> normal, bool pushing, bool manual_correction);

}  // namespace s3::riding::grounded::state::motion
