// Port of skate-game/src/physics/player_input/ground_position.rs.
//! Ground-position observation from TU3 Skateboard::CalculateGroundPosition
//! 82C02840. The four wheel rigid-body centres are expressed in Reckoning752,
//! averaged laterally/longitudinally, and placed at the lowest wheel centre.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/physics/board_runtime.hpp"
#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::player_input::ground_position {

using sc::physics::skeleton_animation_record::AnimationPartTransform;

std::array<std::uint32_t, 4> ground_position(const sc::physics::board_runtime::BoardRuntime& board,
                                             const AnimationPartTransform& ground);

/// Private in Rust (exposed for the ported test).
std::array<float, 4> wheel_ground_position(std::array<std::array<float, 4>, 4> wheels,
                                           const AnimationPartTransform& ground);
std::array<float, 4> rotate(const std::array<std::array<float, 4>, 3>& basis, std::array<float, 4> point);

}  // namespace s3::game::physics::player_input::ground_position
