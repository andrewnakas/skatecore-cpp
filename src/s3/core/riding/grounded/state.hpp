// Port of skate-core/src/riding/grounded/state/mod.rs.
// Readable composition of the TU3 `PhysState_PhysicsGround` lifecycle.
//
// This module owns native state order. Collision queries, skeleton writes and
// other engine objects remain required interfaces; no branch has a no-op
// implementation.
#pragma once
#include "s3/core/riding/grounded/state/board.hpp"
#include "s3/core/riding/grounded/state/board_types.hpp"
#include "s3/core/riding/grounded/state/contact_state.hpp"
#include "s3/core/riding/grounded/state/corrections.hpp"
#include "s3/core/riding/grounded/state/data.hpp"
#include "s3/core/riding/grounded/state/lifecycle.hpp"
#include "s3/core/riding/grounded/state/motion.hpp"
#include "s3/core/riding/grounded/state/ordinary.hpp"
#include "s3/core/riding/grounded/state/output.hpp"
#include "s3/core/riding/grounded/state/post_physics.hpp"
#include "s3/core/riding/grounded/state/update.hpp"

namespace s3::riding::grounded::state {

using data::GroundContactHistory;
using data::GroundUpdateInput;
using data::PhysicsGroundState;

}  // namespace s3::riding::grounded::state
