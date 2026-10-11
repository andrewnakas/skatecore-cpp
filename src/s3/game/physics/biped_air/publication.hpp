// Port of skate-game/src/physics/biped_air/publication.rs
//! Recovered output adapter; switches with the complete lifecycle migration.
#pragma once
#include "s3/core/player/input_phase.hpp"
#include "s3/core/player/offboard/biped_air/recovered.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::biped_air::publication {

void publish(::s3::player::offboard::biped_air::recovered::OffBoardOutput output,
             ::s3::player::input_phase::OffBoardOutputFields& off_board);

}  // namespace s3::game::physics::biped_air::publication
