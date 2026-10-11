// Port of skate-core/src/player/selector/air.rs (whole file).
#include "s3/core/player/selector.hpp"

namespace s3::player::selector {

namespace {
const float STATE_SETTLE_SECONDS = rs::from_bits(0x3DA3D70Au);
const float KNOWN_AIR_COLLISION_GRACE_SECONDS = rs::from_bits(0x3D23D70Au);
const float TRAJECTORY_WIPEOUT_SECONDS = rs::from_bits(0x3D4CCCCDu);
const float TRAJECTORY_EXPIRED_SECONDS = rs::from_bits(0xBE4CCCCDu);
}  // namespace

PhysicalStateId StateSelector::select_physics_air(PhysicalStateId current, const StateSelectionInput& input,
                                                  FrameFacts facts) const {
    const ProcessedStateInput p = input.processed;
    if (facts.wipeout) {
        return PhysicalStateId::WipeoutGround;
    }
    if (p.has_2476(0x8000)) {
        return PhysicalStateId::BipedGround;
    }
    // 82D8B490 lbz2484 reads the high byte of the big-endian flag word.
    if (p.has_2476(0x80) && !p.has_2484(0x01000000)) {
        return PhysicalStateId::BipedAir;
    }
    if (facts.force_known_air) {
        if (facts.colliding || p.has_2468(0x8000)) {
            return PhysicalStateId::WipeoutGround;
        }
        return p.has_2468(0x400) ? PhysicalStateId::KnownAir : current;
    }
    if (p.state_timer_2664 > STATE_SETTLE_SECONDS) {
        if (facts.colliding) {
            if (p.has_2472(4)) {
                return PhysicalStateId::PhysicsAir;
            }
            return facts.skateboard_animated ? PhysicalStateId::GroundAnimation : PhysicalStateId::PhysicsGround;
        }
        if (post_grind_jump_counter > 10) {
            if (facts.grind) {
                return *facts.grind;
            }
        }
    }
    return p.has_2468(0x400) ? PhysicalStateId::KnownAir : current;
}

PhysicalStateId StateSelector::select_known_air(PhysicalStateId current, const StateSelectionInput& input,
                                                FrameFacts facts) const {
    const ProcessedStateInput p = input.processed;
    if (p.has_2480(0x2000000)) {
        return PhysicalStateId::FootPlant;
    }
    if (p.trajectory_collision_time_2772 < TRAJECTORY_WIPEOUT_SECONDS && (p.has_2476(0x80) || p.has_2476(0x8000))) {
        return PhysicalStateId::WipeoutGround;
    }
    // 82D8B5F0 uses the same byte load for the offboard dismount gate.
    if (p.has_2476(0x80) && !p.has_2484(0x01000000)) {
        return PhysicalStateId::BipedAir;
    }
    if (p.has_2476(0x8000)) {
        return PhysicalStateId::BipedGround;
    }
    if (facts.wipeout) {
        return PhysicalStateId::WipeoutGround;
    }
    if (facts.force_known_air) {
        return facts.colliding || p.has_2468(0x8000) ? PhysicalStateId::WipeoutGround : current;
    }
    if (p.state_timer_2664 > STATE_SETTLE_SECONDS && post_grind_jump_counter > 10) {
        if (facts.grind) {
            return *facts.grind;
        }
    }
    if (!facts.colliding || p.state_timer_2664 < KNOWN_AIR_COLLISION_GRACE_SECONDS) {
        if (p.trajectory_collision_time_2772 < TRAJECTORY_EXPIRED_SECONDS) {
            return p.has_2476(0x4000000) ? PhysicalStateId::KnownAir : PhysicalStateId::PhysicsAir;
        }
        if (p.has_2468(0x8000)) {
            return p.has_2476(0x4000000) ? PhysicalStateId::WipeoutGround : PhysicalStateId::PhysicsAir;
        }
        return current;
    }
    if (p.has_2472(4)) {
        return PhysicalStateId::PhysicsAir;
    }
    if (p.has_2480(0x40000) || p.has_2480(0x20000) || p.has_2480(0x10000)) {
        return PhysicalStateId::WipeoutGround;
    }
    if (!p.has_2472(0x8000) && !p.has_2472(0x4000)) {
        return PhysicalStateId::PhysicsGround;
    }
    return PhysicalStateId::GroundAnimation;
}

PhysicalStateId StateSelector::select_grind(PhysicalStateId /*current*/, const StateSelectionInput& input,
                                            FrameFacts facts) const {
    const ProcessedStateInput p = input.processed;
    if (facts.wipeout) {
        return PhysicalStateId::WipeoutGround;
    }
    if (p.has_2484(0x100000)) {
        return PhysicalStateId::PhysicsAirSecondary;
    }
    if (p.has_2476(0x80)) {
        return PhysicalStateId::BipedAir;
    }
    if (p.has_2476(0x8000)) {
        return PhysicalStateId::BipedGround;
    }
    if (p.has_2468(0x100)) {
        return p.air_variant_from_2468();
    }
    return facts.grind.value_or(PhysicalStateId::Nonspecific);
}

}  // namespace s3::player::selector
