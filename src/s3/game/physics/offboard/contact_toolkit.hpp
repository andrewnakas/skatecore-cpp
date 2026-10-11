// Port of skate-game/src/physics/offboard/contact_toolkit.rs
//! Static canonical-world adapter for OffboardGroundAnalyzer.
//! The runtime must supply actual mesh metadata; neither a board contact nor
//! a synthetic ground ray can provide it.
#pragma once
#include "s3/core/player/offboard/contact_toolkit.hpp"
#include "s3/game/physics/offboard/contact_toolkit/world.hpp"

namespace s3::game::physics::offboard::contact_toolkit {

using ::s3::player::offboard::contact_toolkit::Owner;
using world::StaticScene;

}  // namespace s3::game::physics::offboard::contact_toolkit
