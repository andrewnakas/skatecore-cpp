// Port of skate-game/src/physics/biped_ground/feet.rs
//! Ground-only conditioning82D77330 and history82D775D8 around the shared
//! FeetIK evaluator. Air's per-update timer resets do NOT belong here.
#pragma once
#include "s3/core/animation/foot_ik/state.hpp"
#include "s3/core/player/offboard/biped_air/feet.hpp"
#include "s3/core/player/offboard/board_possession/manager.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::biped_ground::feet {

namespace core = ::s3::player::offboard::biped_air::recovered::feet;
using ::s3::animation::foot_ik::state::FootIkState;
using ::s3::player::offboard::board_possession::manager::State;

/// The host supplies actual current bone/line/root observations to core::Input.
/// This borrows state56; it never creates or retains another FeetIK owner.
void update(State& manager, core::Input input, FootIkState& ik);

void update_history(State& s);

}  // namespace s3::game::physics::biped_ground::feet
