// Port of skate-game/src/physics/offboard/air_feet.rs
//! State56 manager updates real foot targets before Skeleton GeneralUpdate.
#pragma once
#include "s3/core/player/input_phase.hpp"
#include "s3/core/player/offboard/board_possession/manager.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/fwd.hpp"

namespace s3::game::physics::offboard::air_feet {

void update_air(SkaterRuntime& skater, sc::player::offboard::board_possession::manager::State& manager);
///82D770D8: retain state56 across500/501/502; clear actual IK reset fields otherwise.
void enter(SkaterRuntime& skater, sc::player::offboard::board_possession::manager::State& manager);
///82D785F8: only these two output bytes belong to the feet service.
void publish(const sc::player::offboard::board_possession::manager::State& manager,
             sc::player::input_phase::OffBoardOutputFields& output);

}  // namespace s3::game::physics::offboard::air_feet
