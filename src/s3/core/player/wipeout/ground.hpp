// Port of skate-core/src/player/wipeout/ground.rs (whole file).
// Complete original Ground checks82D8F9E0 and all its nonexternal leaves.
#pragma once
#include "s3/core/player/wipeout/common.hpp"
#include "s3/core/player/wipeout/data.hpp"
#include "s3/core/player/wipeout/requests.hpp"

namespace s3::player::wipeout::ground {

void check(Requests& state, const Settings& s, const Mode& mode, const Frame& f);
///GroundAnimation82D8F918. These additional checks run even when the shared
///ground check returns during its cooldown. Caller82D34140 supplies scale1.
void check_animation(Requests& state, const Settings& s, const Mode& mode, const Frame& f, float scale);
///82D8FDC0, FootPlant postphysics checks. HandPlant calls check_air(false).
void check_plant(Requests& state, const Settings& s, const Frame& f);

}  // namespace s3::player::wipeout::ground
