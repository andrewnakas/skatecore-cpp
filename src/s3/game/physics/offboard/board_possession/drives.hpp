// Port of skate-game/src/physics/offboard/board_possession/drives.rs
//!74FD8 creates both persistent hand drives. AddDrive reverses argument bodies:
//! child/internal A is skeleton part3/7; parent/internal B is deck6.
#pragma once
#include <array>
#include <cstddef>
#include <vector>

#include "s3/core/physics/assembly.hpp"
#include "s3/core/physics/drive_solver.hpp"
#include "s3/core/player/offboard/board_possession.hpp"
#include "s3/game/physics/offboard/board_possession.hpp"

namespace s3::game::physics::offboard::board_possession::drives {

using sc::physics::assembly::BodySnapshot;
using sc::physics::drive_solver::RetailDriveBodyState;
using sc::physics::drive_solver::RetailDriveRows;

void append(const sc::player::offboard::board_possession::State& state, BodySnapshot deck,
            std::array<BodySnapshot, 2> hands, std::size_t deck_reaction, std::array<std::size_t, 2> hand_reactions,
            float dt, std::vector<RetailDriveRows>& rows);
/// private
RetailDriveBodyState body(BodySnapshot b, std::size_t reaction_index);

}  // namespace s3::game::physics::offboard::board_possession::drives
