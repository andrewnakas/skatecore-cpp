// Port of skate-game/src/physics/grind/wipeout_output.rs.
#include "s3/game/physics/grind/wipeout_output.hpp"

#include "s3/game/physics/skater.hpp"

namespace s3::game::physics::grind::wipeout_output {

/// Call immediately after grind FillOut sets pending_wipeout_impulse, before
/// the completed physical packet is consumed by the next input phase.
void publish(SkaterRuntime& skater) {
    if (skater.grind.pending_wipeout_impulse) {
        const auto impulse = *skater.grind.pending_wipeout_impulse;
        skater.grind.pending_wipeout_impulse.reset();
        auto& output = skater.player_input.physical.physics;
        output.vector_16 = rs::to_bits(impulse);
        output.flag_32 = 1;
    }
    // No inactive clearing: the ordinary physical output reset owns this block.
    // ProcessInput82DB4CCC/4CDC publishes bit2472.11 and vector816; existing
    // Wipeout Enter82D3B5E8 caches them, Update82D3BFEC applies deck force.
    // This is not a direct body impulse/velocity or animation packet mutation.
}

}  // namespace s3::game::physics::grind::wipeout_output
