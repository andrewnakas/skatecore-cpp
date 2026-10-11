// Port of skate-core/src/player/wipeout_state/control_air.rs (whole file).
// Original alternate air-control82D3DD40, including its contact early return.
#pragma once
#include <array>

#include "s3/core/physics/skeleton_body.hpp"
#include "s3/core/player/wipeout_state/data.hpp"
#include "s3/core/player/wipeout_state/math.hpp"

namespace s3::player::wipeout_state::control_air {

bool update(State& state, physics::skeleton_body::SkeletonBody& body, math::V physical_com,
            std::array<float, 2> input, bool contact);

}  // namespace s3::player::wipeout_state::control_air
