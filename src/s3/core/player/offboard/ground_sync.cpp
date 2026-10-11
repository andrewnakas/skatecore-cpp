// Port of skate-core/src/player/offboard/ground_sync.rs (non-template parts).
#include "s3/core/player/offboard/ground_sync.hpp"

namespace s3::player::offboard::ground_sync {

using namespace ground_sync::math;

bool wants_air(const State& state, const CompletedMotion& motion, const Processed& p) {
    bool launch = (motion.contact_flags_368 & 1u) == 0 && p.elapsed_2664 > rs::from_bits(0x3d4ccccdu);
    launch = launch || motion.launch_1076;
    if ((motion.contact_flags_368 & 8u) == 0 && (p.flags_2480 & 0x00020180u) == 0 && !state.flags_144_to_150[6]) {
        launch = launch || (p.flags_2476 & 0x00080000u) != 0;
    }
    return launch;
}

Bounds board_bounds(const Frame& frame, Vector offset, Vector extents) {
    Vector forward = frame[2];
    forward[1] = 0.0f;
    forward = normalize_or(forward, Vector{0.0f, 0.0f, 1.0f, 0.0f});
    const Vector up{0.0f, 1.0f, 0.0f, 0.0f};
    const Vector right = cross(up, forward);
    const Vector position = madd(up, offset[1], madd(forward, offset[2] + extents[2], frame[3]));
    return Bounds{{right, up, forward, position}, extents};
}

}  // namespace s3::player::offboard::ground_sync
