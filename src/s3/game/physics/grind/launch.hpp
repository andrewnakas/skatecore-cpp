// Port of skate-game/src/physics/grind/launch.rs.
//! Concrete SetVelocity82C04168 continuation; retain main's jumper owner.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/physics/board_runtime.hpp"
#include "s3/core/physics/grind_contact/manager.hpp"
#include "s3/core/physics/grind_forces/launch.hpp"

namespace s3::game::physics::grind::launch {

std::array<float, 4> apply(::s3::physics::board_runtime::BoardRuntime& board,
                           ::s3::physics::grind_contact::manager::Jumper& jumper, std::uint32_t& flags_2476,
                           ::s3::physics::grind_forces::launch::Input input);

}  // namespace s3::game::physics::grind::launch
