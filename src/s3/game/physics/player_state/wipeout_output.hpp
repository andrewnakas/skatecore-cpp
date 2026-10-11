// Port of skate-game/src/physics/player_state/wipeout_output.rs.
//! Wipeout300 Fill82D3EEE8 writes the shared completed physical packet.
#pragma once
#include "s3/game/common.hpp"
#include "s3/game/physics/fwd.hpp"

namespace s3::game::physics::player_state::wipeout_output {

/// `pub(super)`.
void publish(SkaterRuntime& skater);

}  // namespace s3::game::physics::player_state::wipeout_output
