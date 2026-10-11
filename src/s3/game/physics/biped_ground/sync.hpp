// Port of skate-game/src/physics/biped_ground/sync.rs
//! Ground-owned Sync stages. The caller executes returned launch on its ONE
//! persistent AirSelector before frame correction; no speculative transition.
#pragma once
#include "s3/core/player/offboard/air_launch.hpp"
#include "s3/core/player/offboard/ground_entry.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::biped_ground::sync {

namespace air_launch = ::s3::player::offboard::air_launch;
using ::s3::player::offboard::ground_entry::Vector;

struct LaunchInput {
    air_launch::Processed processed{};
    float elapsed_2664 = 0.0f;
    Vector skeleton_point_10960{};
};

// `impl Owner { fn prepare_air }` lives in biped_ground.hpp (Owner); body in sync.cpp.

}  // namespace s3::game::physics::biped_ground::sync
