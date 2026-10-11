// Port of skate-core/src/player/selector/mod.rs.
#include "s3/core/player/selector.hpp"

namespace s3::player::selector {

using conditions::condition_is_off_ground;
using conditions::condition_is_off_ground_skitching;
using conditions::is_skateboard_animated;

namespace {
/// Rust `i32::wrapping_add` / `wrapping_sub` by one.
std::int32_t wrapping_inc(std::int32_t v) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(v) + 1u);
}
std::int32_t wrapping_dec(std::int32_t v) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(v) - 1u);
}
}  // namespace

PhysicalStateId StateSelector::calculate(PhysicalStateId current, const StateSelectionInput& input) {
    const ProcessedStateInput p = input.processed;
    if (teleport_countdown > 0) {
        teleport_countdown = wrapping_dec(teleport_countdown);
    }
    current_state = current;
    revert_exited_normally = false;
    request_teleport = false;

    const bool colliding = p.wheel_contact_count_2556 > 0 || p.has_2468(0x10000) || p.has_2468(0x20000);
    nonspecific_collision_free_frames = current == PhysicalStateId::Nonspecific && !colliding
                                            ? wrapping_inc(nonspecific_collision_free_frames)
                                            : 0;
    nonspecific_collision_frames =
        current == PhysicalStateId::Nonspecific && colliding ? wrapping_inc(nonspecific_collision_frames) : 0;
    two_wheel_counter = input.skateboard_contact_count_869 > 1 ? wrapping_inc(two_wheel_counter) : 0;
    three_wheel_counter = input.skateboard_contact_count_869 > 2 ? wrapping_inc(three_wheel_counter) : 0;
    something_colliding_frames = colliding ? wrapping_inc(something_colliding_frames) : 0;
    const bool special_grind = p.category_2512 == 400 && p.has_2468(0x400000);
    post_grind_jump_counter = special_grind ? 0 : wrapping_inc(post_grind_jump_counter);
    if (p.category_2512 == 200) {
        air_frames = p.has_2468(8) ? air_frames : wrapping_inc(air_frames);
    } else {
        air_frames = 0;
    }
    if (current == PhysicalStateId::Skitching) {
        skitch_exit_countdown = 10;
    } else {
        skitch_exit_countdown = skitch_exit_countdown > 0 ? wrapping_dec(skitch_exit_countdown) : 0;
    }

    if (p.has_2468(2) || p.has_2472(0x40000)) {
        return PhysicalStateId::Teleporting;
    }
    if (air_frames > 300) {
        request_teleport = true;
        return current;
    }
    if (p.field_1776 < 0 && (static_cast<std::uint32_t>(p.field_1776) & 0x2000000u) == 0 &&
        teleport_countdown == 0) {
        return PhysicalStateId::FollowPath;
    }

    FrameFacts facts;
    facts.colliding = colliding;
    facts.wipeout = p.has_2468(0x40000);
    facts.force_known_air = p.has_2472(0x40);
    facts.skateboard_animated = is_skateboard_animated(input.skeleton);
    facts.off_ground = condition_is_off_ground(input.board_body, input.normal_off_ground);
    facts.off_ground_skitching = condition_is_off_ground_skitching(input.board_body, input.skitching_off_ground);
    facts.grind = check_for_grind(p);

    switch (current) {
        case PhysicalStateId::PhysicsGround:
        case PhysicalStateId::SlideGround:
        case PhysicalStateId::RevertGround:
        case PhysicalStateId::GroundAnimation:
        case PhysicalStateId::Skitching:
        case PhysicalStateId::FollowPath:
            return select_ground_family(current, input, facts);
        case PhysicalStateId::PhysicsAir:
            return select_physics_air(current, input, facts);
        case PhysicalStateId::KnownAir:
            return select_known_air(current, input, facts);
        case PhysicalStateId::PhysicsAirSecondary:
            if (facts.wipeout) {
                return PhysicalStateId::WipeoutGround;
            } else if (!p.has_2484(0x100000)) {
                return PhysicalStateId::PhysicsAir;
            } else {
                return current;
            }
        case PhysicalStateId::WipeoutGround:
        case PhysicalStateId::Sleeping:
        case PhysicalStateId::Teleporting:
            return current;
        case PhysicalStateId::GrindBoardslide:
        case PhysicalStateId::GrindFiftyFifty:
        case PhysicalStateId::GrindTipslide:
        case PhysicalStateId::GrindFiveO:
        case PhysicalStateId::GrindBackslash:
        case PhysicalStateId::GrindDarkslide:
            return select_grind(current, input, facts);
        case PhysicalStateId::BipedGround:
        case PhysicalStateId::BipedAir:
        case PhysicalStateId::OffBoardPushing:
        case PhysicalStateId::LandingOnDeck:
        case PhysicalStateId::HandPlant:
        case PhysicalStateId::FootPlant:
        case PhysicalStateId::Boneless:
            return select_biped_or_plant(current, input, facts);
        case PhysicalStateId::Nonspecific:
            return select_nonspecific(current, input, facts);
    }
    S3_PANIC("unknown PhysicalStateId");
}

PhysicalStateId StateSelector::select_nonspecific(PhysicalStateId current, const StateSelectionInput& input,
                                                  FrameFacts facts) const {
    const ProcessedStateInput p = input.processed;
    if (p.has_2484(0x100000)) {
        return PhysicalStateId::PhysicsAirSecondary;
    }
    if (facts.wipeout) {
        return PhysicalStateId::WipeoutGround;
    }
    if (p.has_2468(0x100)) {
        return p.air_variant_from_2468();
    }
    if (p.has_2468(0x400000)) {
        return current;
    }
    if (p.has_2472(8)) {
        return p.has_2484(0x200000) ? PhysicalStateId::WipeoutGround : PhysicalStateId::PhysicsAir;
    }
    if (p.has_2472(4)) {
        return current;
    }
    if (facts.grind) {
        return *facts.grind;
    }
    if ((p.grind_investigation_flags_1516 & 0x8000000u) != 0) {
        if (three_wheel_counter > 10) {
            return PhysicalStateId::PhysicsGround;
        }
    } else if (two_wheel_counter > 2) {
        return facts.skateboard_animated ? PhysicalStateId::GroundAnimation : PhysicalStateId::PhysicsGround;
    }
    if (nonspecific_collision_free_frames > 2) {
        return PhysicalStateId::PhysicsAir;
    }
    const std::int32_t collision_exit_frames = p.has_2484(0x200000) ? 30 : 60;
    if (nonspecific_collision_frames > collision_exit_frames) {
        return PhysicalStateId::PhysicsGround;
    }
    return current;
}

std::optional<PhysicalStateId> check_for_grind(ProcessedStateInput input) {
    if (!input.grind_candidate_1488 || input.has_2480(0x20000) || input.has_2480(0x4000000)) {
        return std::nullopt;
    }
    switch (input.grind_type_1248) {
        case 0: return PhysicalStateId::GrindFiftyFifty;
        case 1: return PhysicalStateId::GrindBoardslide;
        case 2: return PhysicalStateId::GrindTipslide;
        case 3: return PhysicalStateId::GrindFiveO;
        case 4: return PhysicalStateId::GrindBackslash;
        case 5: return PhysicalStateId::GrindDarkslide;
        default: return std::nullopt;
    }
}

}  // namespace s3::player::selector
