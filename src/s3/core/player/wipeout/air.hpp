// Port of skate-core/src/player/wipeout/air.rs (whole file).
// Complete Air checks82D90358 including bad landing82D90D08.
#pragma once
#include "s3/core/player/wipeout/common.hpp"
#include "s3/core/player/wipeout/data.hpp"
#include "s3/core/player/wipeout/requests.hpp"

namespace s3::player::wipeout::air {

///82D8FEE8, shared off-board air collision check; no on-board cooldown/landing tests.
void check_collision(Requests& state, const Settings& s, const Mode& mode, const Frame& f);
void check(Requests& state, const Settings& s, const Mode& mode, const Frame& f, bool use_com);

}  // namespace s3::player::wipeout::air
