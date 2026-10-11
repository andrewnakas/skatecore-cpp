// Port of skate-game/src/physics/biped_ground/entry.rs
//! Effective animation root82BE3650, called by Ground Enter at82D30848.
#pragma once
#include <array>
#include <cstdint>

#include "s3/game/common.hpp"

namespace s3::game::physics::biped_ground::entry {

using Frame = std::array<std::array<float, 4>, 4>;

///The source multiplies every lane of X/Z by -1, not the position or up.
Frame effective_root(Frame frame, std::uint32_t flags_2476);

}  // namespace s3::game::physics::biped_ground::entry
