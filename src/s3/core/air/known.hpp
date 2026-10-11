// Port of skate-core/src/air/known/mod.rs.
// Readable TU3 `PhysState_KnownAir` (state 201).
//
// The routines in this module preserve the observed Skate 3 call, branch,
// field, and constant order. Engine-owned objects and Xenon-specific vector
// operations are mandatory adapter contracts; this module supplies no
// guessed fallbacks.
#pragma once
#include "s3/core/air/known/body.hpp"
#include "s3/core/air/known/data.hpp"
#include "s3/core/air/known/footplant.hpp"
#include "s3/core/air/known/lifecycle.hpp"
#include "s3/core/air/known/output.hpp"
#include "s3/core/air/known/post_physics.hpp"
#include "s3/core/air/known/runtime.hpp"
#include "s3/core/air/known/trajectory.hpp"
#include "s3/core/air/known/update.hpp"

namespace s3::air::known {

using body::calculate_body_flip_speed;
using body::calculate_body_spin_speed;
using data::AffineTransform;
using data::GrindFootBody;
using data::KnownAirFootplantInput;
using data::KnownAirFrame;
using data::KnownAirModeSettings;
using data::KnownAirOutput;
using data::KnownAirPrediction;
using data::KnownAirReckoningFields;
using data::KnownAirSettings;
using data::KnownAirState;
using data::KnownAirTrajectory;
using data::KnownAirWipeoutRequest;
using data::KnownAirWipeoutSettings;
using data::RestoreVelocityGeometry;
using data::Vector4;
using lifecycle::enter;
using lifecycle::exit;
using output::fill_physics_output;
using post_physics::check_upside_down_falling_wipeout;
using post_physics::update_post_physics;
using runtime::KnownAirMath;
using runtime::KnownAirRuntime;
using trajectory::init_trajectory_info;
using trajectory::restore_velocity;
using trajectory::update_trajectory_follow;
using update_mod::update;

}  // namespace s3::air::known
