// Port of skate-core/src/player/wipeout_state/recovery.rs (whole file).
// Original recovery82D3EAE0, request82D3E980, scalar82D3E5E8 and post82D3ED30.
// The `impl State` methods are declared in data.hpp and defined in recovery.cpp.
#pragma once
#include "s3/core/player/wipeout_state/data.hpp"

namespace s3::player::wipeout_state::recovery {

struct Settings {
    float minimum_time = 0.0f;       // physics_wipeout280
    float minimum_settled = 0.0f;    // 284
    float maximum_time = 0.0f;       // 292
    float fade_time = 0.0f;          // 296
    float over_speed = 0.0f;         // 80
    float over_minimum_time = 0.0f;  // 84
};

}  // namespace s3::player::wipeout_state::recovery
