// Port of skate-core/src/player/wipeout/mod.rs.
// Original TU3 shared Wipeout request calculation and persistent history.
// Physical bail/recovery state response is scheduled by the game coordinator.
//
// PARTIAL PORT (stage C): only items used by physics (data, requests, common/regional_force).
// air.rs, ground.rs and tests.rs are left to the player stage.
#pragma once
#include "s3/core/player/wipeout/common.hpp"
#include "s3/core/player/wipeout/data.hpp"
#include "s3/core/player/wipeout/requests.hpp"

namespace s3::player::wipeout {
// pub(crate) use common::force as regional_force;
inline bool regional_force(const Frame& frame, float body, float arms) {
    return common::force(frame, body, arms);
}
}  // namespace s3::player::wipeout
