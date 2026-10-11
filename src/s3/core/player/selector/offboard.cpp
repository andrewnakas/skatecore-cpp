// Port of skate-core/src/player/selector/offboard.rs (whole file).
#include "s3/core/player/selector.hpp"

namespace s3::player::selector {

namespace {
const float HANDPLANT_GROUND_DELAY_SECONDS = rs::from_bits(0x3E4CCCCDu);
}  // namespace

PhysicalStateId StateSelector::select_biped_or_plant(PhysicalStateId current, const StateSelectionInput& input,
                                                     FrameFacts facts) const {
    const ProcessedStateInput p = input.processed;
    switch (current) {
        case PhysicalStateId::BipedGround:
            if (facts.wipeout) {
                return PhysicalStateId::WipeoutGround;
            }
            if (p.has_2484(0x10000)) {
                return PhysicalStateId::BipedAir;
            }
            if (p.has_2476(0x200000)) {
                return PhysicalStateId::OffBoardPushing;
            }
            if (p.has_2476(0x8000) || p.has_2480(0x40000)) {
                return current;
            }
            if (!p.has_2484(1) && !p.has_2488(0x10000000) && !p.has_2488(0x80000000u)) {
                return PhysicalStateId::PhysicsAir;
            }
            return PhysicalStateId::PhysicsGround;
        case PhysicalStateId::BipedAir:
            if (facts.wipeout) {
                return PhysicalStateId::WipeoutGround;
            }
            if (!p.has_2480(0x10)) {
                // 82D8B8D4 loads a BIG-ENDIAN halfword at2484, then
                // B8D8 tests its low bit: bit16 of the stored u32.
                // This is OffBoard328, not BipedBoardOnGround bit0.
                if (p.has_2484(0x10000)) {
                    if (p.has_2476(0x8000) || p.has_2476(0x80)) {
                        return current;
                    }
                    return PhysicalStateId::PhysicsAir;
                }
                return PhysicalStateId::BipedGround;
            }
            return PhysicalStateId::LandingOnDeck;
        case PhysicalStateId::OffBoardPushing:
            if (facts.wipeout) {
                return PhysicalStateId::WipeoutGround;
            } else if (!p.has_2476(0x200000)) {
                return PhysicalStateId::BipedGround;
            }
            return current;
        case PhysicalStateId::LandingOnDeck:
            if (facts.wipeout) {
                return PhysicalStateId::WipeoutGround;
            } else if (p.has_2476(0x10000)) {
                return PhysicalStateId::PhysicsGround;
            } else if (p.has_2476(0x80)) {
                return PhysicalStateId::BipedAir;
            }
            return current;
        case PhysicalStateId::HandPlant:
            if (facts.wipeout) {
                return PhysicalStateId::WipeoutGround;
            } else if ((facts.colliding && p.state_timer_2664 > HANDPLANT_GROUND_DELAY_SECONDS) ||
                       static_cast<std::int32_t>(p.flags_2480) >= 0) {
                return PhysicalStateId::PhysicsGround;
            }
            return current;
        case PhysicalStateId::FootPlant:
            if (facts.wipeout) {
                return PhysicalStateId::WipeoutGround;
                // 82D8B970 loads the first byte of the big-endian word,
                // then tests its low bit: full-word bit24 is Air448.
            } else if (!p.has_2480(0x01000000)) {
                return p.air_variant_from_2468();
            }
            return current;
        case PhysicalStateId::Boneless:
            if (facts.wipeout) {
                return PhysicalStateId::WipeoutGround;
            }
            if (p.has_2468(0x400)) {
                return PhysicalStateId::KnownAir;
            }
            if (p.has_2476(0x80)) {
                return PhysicalStateId::BipedAir;
            }
            if (!p.has_2480(0x4000) && !p.has_2480(0x2000)) {
                return PhysicalStateId::PhysicsAir;
            }
            return current;
        default:
            S3_PANIC("off-board/plant dispatch received another state");
    }
}

}  // namespace s3::player::selector
