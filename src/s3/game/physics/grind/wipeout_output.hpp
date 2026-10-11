// Port of skate-game/src/physics/grind/wipeout_output.rs.
//! Complete the grind FillOut write to native PhysOut pointer+12's 48-byte
//! block. CURRENT already represents it as PhysicalPlayerInput::physics.
#pragma once
#include "s3/game/physics/fwd.hpp"

namespace s3::game::physics::grind::wipeout_output {

/// Call immediately after grind FillOut sets pending_wipeout_impulse, before
/// the completed physical packet is consumed by the next input phase.
void publish(SkaterRuntime& skater);

}  // namespace s3::game::physics::grind::wipeout_output
