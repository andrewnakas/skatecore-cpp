// Port of skate-core/src/player/selector/ground.rs (whole file).
#include "s3/core/player/selector.hpp"

namespace s3::player::selector {

PhysicalStateId StateSelector::select_ground_family(PhysicalStateId current, const StateSelectionInput& input,
                                                    FrameFacts facts) {
    const ProcessedStateInput p = input.processed;
    switch (current) {
        case PhysicalStateId::PhysicsGround: {
            if (p.has_2480(0x1000) && (!p.has_2480(0x400) || !p.has_2480(0x200))) {
                // TU3 calls `sk83_na_f_01a4` at 0x82D8B23C here. The
                // target is 0x82B61BB8 and consists solely of `blr`.
                return PhysicalStateId::LandingOnDeck;
            }
            const bool handplant_contact = p.has_2480(0x40000000) && input.skateboard_contact_count_869 <= 2;
            if (p.has_2476(1) && (handplant_contact || input.skateboard_contact_count_869 == 0)) {
                return PhysicalStateId::HandPlant;
            }
            if (p.has_2476(0x8000)) {
                return PhysicalStateId::BipedGround;
            }
            if (p.has_2476(0x80)) {
                return PhysicalStateId::BipedAir;
            }
            if (facts.wipeout) {
                return PhysicalStateId::WipeoutGround;
            }
            if (p.field_2572 == 1) {
                return p.air_variant_from_2468();
            }
            if (p.field_2744 != 0.0f) {
                return PhysicalStateId::RevertGround;
            }
            if (facts.grind) {
                return *facts.grind;
            }
            if (!facts.colliding && facts.off_ground && teleport_countdown == 0) {
                return PhysicalStateId::PhysicsAir;
            }
            if (facts.skateboard_animated && teleport_countdown == 0) {
                return PhysicalStateId::GroundAnimation;
            }
            if (p.has_2476(0x200000) && p.has_2480(0x400000) && skitch_exit_countdown == 0) {
                return PhysicalStateId::Skitching;
            }
            return p.field_2732 != 0.0f ? PhysicalStateId::SlideGround : current;
        }
        case PhysicalStateId::SlideGround: {
            if (facts.wipeout) {
                return PhysicalStateId::WipeoutGround;
            }
            if (p.field_2744 != 0.0f) {
                return PhysicalStateId::RevertGround;
            }
            if (facts.grind) {
                return *facts.grind;
            }
            if (!facts.colliding && facts.off_ground) {
                return PhysicalStateId::PhysicsAir;
            }
            return p.field_2732 == 0.0f ? PhysicalStateId::PhysicsGround : current;
        }
        case PhysicalStateId::RevertGround: {
            if (!facts.colliding && facts.off_ground) {
                return PhysicalStateId::PhysicsAir;
            }
            if (p.has_2468(0x10000) || p.has_2472(0x20000)) {
                return PhysicalStateId::PhysicsGround;
            }
            if (!p.has_2472(0x200000)) {
                revert_exited_normally = true;
                return PhysicalStateId::PhysicsGround;
            }
            return facts.wipeout ? PhysicalStateId::WipeoutGround : current;
        }
        case PhysicalStateId::GroundAnimation: {
            if (facts.wipeout) {
                return PhysicalStateId::WipeoutGround;
            }
            if (p.has_2480(0x4000)) {
                return PhysicalStateId::Boneless;
            }
            if (p.has_2476(0x8000)) {
                return PhysicalStateId::BipedGround;
            }
            if (p.has_2476(0x80)) {
                return PhysicalStateId::BipedAir;
            }
            if (p.field_2572 == 1) {
                return p.air_variant_from_2468();
            }
            if (!facts.colliding && facts.off_ground) {
                return PhysicalStateId::PhysicsAir;
            }
            return facts.skateboard_animated ? current : PhysicalStateId::PhysicsGround;
        }
        case PhysicalStateId::Skitching: {
            if (!p.has_2476(0x200000)) {
                return PhysicalStateId::PhysicsGround;
            }
            if (facts.wipeout) {
                return PhysicalStateId::WipeoutGround;
            }
            return facts.off_ground_skitching && teleport_countdown == 0 ? PhysicalStateId::PhysicsGround : current;
        }
        case PhysicalStateId::FollowPath:
            return (static_cast<std::uint32_t>(p.field_1776) & 0x2000000u) != 0 ? PhysicalStateId::PhysicsGround
                                                                                 : current;
        default:
            S3_PANIC("ground-family dispatch accepts only states 100..105");
    }
}

}  // namespace s3::player::selector
