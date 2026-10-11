// Port of skate-game/src/physics/offboard/mod.rs
//! Off-board game adapters over the shared physical and scene owners.
//!
//! These modules are the source-backed implementation units for the off-board
//! owner. The coordinator calls them through the selected physical state.
//
// Orphan .rs files in physics/offboard/ that mod.rs does not declare (runtime.rs,
// air_diagnostics.rs, air_state.rs, air_trajectory.rs, board_effects.rs, collision_settings.rs,
// feet.rs, ground_input.rs, ground_state.rs, hand_drives.rs, possession.rs, settings/possession.rs)
// are dead code and not ported.
#pragma once
#include "s3/game/physics/offboard/air_feet.hpp"
#include "s3/game/physics/offboard/air_selector.hpp"
#include "s3/game/physics/offboard/board_manager.hpp"
#include "s3/game/physics/offboard/board_possession.hpp"
#include "s3/game/physics/offboard/contact_queries.hpp"
#include "s3/game/physics/offboard/contact_toolkit.hpp"
#include "s3/game/physics/offboard/grab_scene.hpp"
#include "s3/game/physics/offboard/ground_geometry.hpp"
#include "s3/game/physics/offboard/ground_query.hpp"
#include "s3/game/physics/offboard/ground_sync.hpp"
#include "s3/game/physics/offboard/landing_deck.hpp"
#include "s3/game/physics/offboard/pose_adjust.hpp"
#include "s3/game/physics/offboard/post_physics.hpp"
#include "s3/game/physics/offboard/settings.hpp"
#include "s3/game/physics/offboard/skeleton_air.hpp"
#include "s3/game/physics/offboard/skeleton_ground.hpp"
