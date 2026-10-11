// Port of skate-core/src/player/wipeout/mod.rs.
// Original TU3 shared Wipeout request calculation and persistent history.
// Physical bail/recovery state response is scheduled by the game coordinator.
//
#pragma once
#include "s3/core/player/wipeout/air.hpp"
#include "s3/core/player/wipeout/common.hpp"
#include "s3/core/player/wipeout/data.hpp"
#include "s3/core/player/wipeout/ground.hpp"
#include "s3/core/player/wipeout/requests.hpp"

namespace s3::player::wipeout {
// pub use air::check as check_air; pub use air::check_collision as check_air_collision;
inline void check_air(Requests& state, const Settings& s, const Mode& mode, const Frame& f, bool use_com) {
    air::check(state, s, mode, f, use_com);
}
inline void check_air_collision(Requests& state, const Settings& s, const Mode& mode, const Frame& f) {
    air::check_collision(state, s, mode, f);
}
// pub use ground::{check as check_ground, check_animation as check_ground_animation, check_plant};
inline void check_ground(Requests& state, const Settings& s, const Mode& mode, const Frame& f) {
    ground::check(state, s, mode, f);
}
inline void check_ground_animation(Requests& state, const Settings& s, const Mode& mode, const Frame& f,
                                   float scale) {
    ground::check_animation(state, s, mode, f, scale);
}
using ground::check_plant;
// pub(crate) use common::force as regional_force;
inline bool regional_force(const Frame& frame, float body, float arms) {
    return common::force(frame, body, arms);
}
}  // namespace s3::player::wipeout
