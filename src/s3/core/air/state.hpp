// Port of skate-core/src/air/state/mod.rs.
// Readable TU3 `PhysState_PhysicsAir` lifecycle and update composition.
//
// The state owns the recovered branch/order semantics. Calls into the board,
// skeleton, trajectory selector, wipeout controller, and unresolved Xenon
// vector math remain mandatory runtime interfaces with no fallback.
#pragma once
#include "s3/core/air/state/board.hpp"
#include "s3/core/air/state/data.hpp"
#include "s3/core/air/state/jump_velocity.hpp"
#include "s3/core/air/state/lifecycle.hpp"
#include "s3/core/air/state/math.hpp"
#include "s3/core/air/state/post_physics.hpp"
#include "s3/core/air/state/runtime.hpp"
#include "s3/core/air/state/update.hpp"

namespace s3::air::state {

using board::update_skateboard;
using data::AirBoardForce;
using data::AirTrajectory;
using data::PhysicsAirFrame;
using data::PhysicsAirOutput;
using data::PhysicsAirReckoningFields;
using data::PhysicsAirSettings;
using data::PhysicsAirState;
using jump_velocity::calculate_velocity_from_jump;
using jump_velocity::PhysicsAirMath;
using lifecycle::enter;
using lifecycle::exit;
using lifecycle::fill_physics_output;
using math::AirMath;
using math::angle_between_vectors;
using math::clamp_jump_velocity;
using post_physics::update_post_physics;
using runtime::PhysicsAirLaunchInfo;
using runtime::PhysicsAirRuntime;
using update_mod::integrate_trajectory_fixed_step;
using update_mod::update;
using update_mod::wrap_signed_angle;

}  // namespace s3::air::state
