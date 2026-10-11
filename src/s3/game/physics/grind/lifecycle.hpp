// Port of skate-game/src/physics/grind/lifecycle.rs.
//! Physical side effects of Enter82D3F318/Exit82D3F430, all six families.
//! State latches/frame initialization are distinct from these body operations.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

#include "s3/core/physics/board_runtime.hpp"
#include "s3/core/physics/skeleton_body.hpp"

namespace s3::game::physics::grind::lifecycle {

using ::s3::physics::board_runtime::BoardRuntime;
using ::s3::physics::skeleton_body::collision_mode::SkeletonCollisionMode;

inline constexpr std::array<std::size_t, 4> FEET = {19, 15, 20, 16};

void enter(BoardRuntime& board, std::uint8_t& animated_290, bool& skeleton_16505, SkeletonCollisionMode& collision);

void exit(BoardRuntime& board, std::uint8_t& animated_290, SkeletonCollisionMode& collision,
          // BoneHasCollision values from CURRENT's stock part definitions in FEET order.
          std::array<bool, 4> bone_has_collision,
          // Existing82C091F8 standard setting, already scaled by822F860C.
          float standard_angular_drag);

///82D3F710. These are actual wheel collision bytes844..847.
void manage_wheel_spin(BoardRuntime& board, std::array<bool, 4> contact);

///82D3F7D0/S2 82D85228 SetPartAngularDrag; ZIP's linear_drag write was wrong.
void clear_wheel_spin(BoardRuntime& board);

}  // namespace s3::game::physics::grind::lifecycle
