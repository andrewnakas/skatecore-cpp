// Port of skate-game/src/graph_host/mod.rs.
//! Production graph host building blocks. Root owns schedule and registration.
// Offboard graph leaves remain parsed/source-backed but are not registered as
// live state owners until the corresponding Biped services are integrated.
//
// Every module declared by mod.rs is included below (the `#[path]` ones under their module
// names: motion_dismount, motion_offboard_cadence, motion_push_off, motion_reset, motion_runout,
// motion_toggle_board, motion_twist_lean). motion.rs's own `#[path]` submodules live under
// graph_host/motion/ and are reached through motion.hpp.
#pragma once
#include "s3/game/graph_host/action.hpp"
#include "s3/game/graph_host/action_conditions.hpp"
#include "s3/game/graph_host/action_nodes.hpp"
#include "s3/game/graph_host/condition_nodes.hpp"
#include "s3/game/graph_host/crouching_settings.hpp"
#include "s3/game/graph_host/motion.hpp"
#include "s3/game/graph_host/motion_air_leg.hpp"
#include "s3/game/graph_host/motion_animation.hpp"
#include "s3/game/graph_host/motion_bump.hpp"
#include "s3/game/graph_host/motion_channels.hpp"
#include "s3/game/graph_host/motion_character_gesture.hpp"
#include "s3/game/graph_host/motion_conditions.hpp"
#include "s3/game/graph_host/motion_dismount.hpp"
#include "s3/game/graph_host/motion_finger_flip.hpp"
#include "s3/game/graph_host/motion_gameplay_conditions.hpp"
#include "s3/game/graph_host/motion_grind.hpp"
#include "s3/game/graph_host/motion_ground_slope.hpp"
#include "s3/game/graph_host/motion_hand_services.hpp"
#include "s3/game/graph_host/motion_hippy_jump.hpp"
#include "s3/game/graph_host/motion_hooks.hpp"
#include "s3/game/graph_host/motion_intent_filter.hpp"
#include "s3/game/graph_host/motion_kickturn.hpp"
#include "s3/game/graph_host/motion_landing.hpp"
#include "s3/game/graph_host/motion_landing_execute.hpp"
#include "s3/game/graph_host/motion_manual.hpp"
#include "s3/game/graph_host/motion_native.hpp"
#include "s3/game/graph_host/motion_nodes.hpp"
#include "s3/game/graph_host/motion_offboard_cadence.hpp"
#include "s3/game/graph_host/motion_push_off.hpp"
#include "s3/game/graph_host/motion_reset.hpp"
#include "s3/game/graph_host/motion_riding.hpp"
#include "s3/game/graph_host/motion_riding_conditions.hpp"
#include "s3/game/graph_host/motion_runout.hpp"
#include "s3/game/graph_host/motion_scoring_trick.hpp"
#include "s3/game/graph_host/motion_shove.hpp"
#include "s3/game/graph_host/motion_sliding.hpp"
#include "s3/game/graph_host/motion_spin.hpp"
#include "s3/game/graph_host/motion_stock_conditions.hpp"
#include "s3/game/graph_host/motion_stock_gameplay.hpp"
#include "s3/game/graph_host/motion_toggle_board.hpp"
#include "s3/game/graph_host/motion_tricks.hpp"
#include "s3/game/graph_host/motion_twist_lean.hpp"
#include "s3/game/graph_host/motion_wipeout.hpp"
#include "s3/game/graph_host/outputs.hpp"
#include "s3/game/graph_host/pumping_settings.hpp"
#include "s3/game/graph_host/pushing.hpp"
#include "s3/game/graph_host/pushing_settings.hpp"
#include "s3/game/graph_host/turning_settings.hpp"

namespace s3::game::graph_host {

/// `pub(crate) use condition_nodes::numeric as parse_numeric_condition;`
inline s3::graph::conditions::NumericCondition parse_numeric_condition(
    const s3::data::state_graph_attributes::Attributes& attributes) {
    return condition_nodes::numeric(attributes);
}

}  // namespace s3::game::graph_host
