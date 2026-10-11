// Port of skate-core/src/player/wipeout_state/control_profile_air.rs (whole file).
// Original profile-driven Wipeout air rotation82D3CB50.
#pragma once
#include <array>

#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/core/physics/skeleton_body.hpp"
#include "s3/core/player/wipeout_state/data.hpp"
#include "s3/core/player/wipeout_state/math.hpp"
#include "s3/core/player/wipeout_state/profiles.hpp"

namespace s3::player::wipeout_state::control_profile_air {

void update(State& state, physics::skeleton_body::SkeletonBody& body, math::V physical_com,
            const physics::skeleton_animation_record::AnimationPartTransform& effective, std::array<float, 2> input,
            const profiles::Profile& profile);

}  // namespace s3::player::wipeout_state::control_profile_air
