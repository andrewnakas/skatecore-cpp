// Port of skate-game/src/physics/solve/assembly_contacts.rs.
//! Original single-player SkaterSkaterCollisionPipeline8276556C.
//! Board construction precedes Skeleton construction in82DB18A0. Preserve
//! that assembly order: board self (all culled), board/rider, rider self.
#pragma once
#include <cstdint>
#include <vector>

#include "s3/core/physics/board_step.hpp"
#include "s3/core/physics/board_world.hpp"
#include "s3/core/physics/skeleton_body.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::solve::assembly_contacts {

Status append(std::vector<sc::physics::board_step::BoardCollision>& contacts,
              const std::vector<sc::physics::board_world::BoardWorldVolume>& board,
              const std::vector<sc::physics::board_world::BoardWorldVolume>& rider, std::uint32_t board_group,
              const sc::physics::skeleton_body::SkeletonCollisionMode& collision);

/// Private in Rust.
bool group_pair_allowed(std::uint32_t a, std::uint32_t b);

void append_pair(std::vector<sc::physics::board_step::BoardCollision>& contacts,
                 const sc::physics::board_world::BoardWorldVolume& a,
                 const sc::physics::board_world::BoardWorldVolume& b);

}  // namespace s3::game::physics::solve::assembly_contacts
