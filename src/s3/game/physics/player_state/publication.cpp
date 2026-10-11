// Port of skate-game/src/physics/player_state/publication.rs.
//! Selected-state FillPhysOut and common state82DB7580..7790.
#include "s3/game/physics/player_state/publication.hpp"

#include "s3/core/air/state.hpp"
#include "s3/core/physics/contact_feedback.hpp"
#include "s3/core/physics/filtered_state.hpp"
#include "s3/game/physics.hpp"
#include "s3/game/physics/biped_air.hpp"
#include "s3/game/physics/biped_ground.hpp"
#include "s3/game/physics/grind.hpp"
#include "s3/game/physics/known_air.hpp"
#include "s3/game/physics/landing_on_deck.hpp"
#include "s3/game/physics/offboard/board_manager/runtime.hpp"
#include "s3/game/physics/player_state/wipeout_output.hpp"
#include "s3/game/physics/skater.hpp"
#include "s3/game/state_names.hpp"

namespace s3::game::physics::player_state::publication {

using sc::physics::filtered_state::FilteredStateInput;
using sc::physics::filtered_state::GrindState;
using sc::player::input_phase::CurrentStateFields;
using sc::player::state::PhysicalStateId;
namespace state_ns = sc::player::state;

namespace {
bool has_output_owner(PhysicalStateId s) {
    if (state_ns::is_grind(s) || s == PhysicalStateId::Nonspecific) return true;
    switch (s) {
        case PhysicalStateId::PhysicsGround:
        case PhysicalStateId::PhysicsAir:
        case PhysicalStateId::PhysicsAirSecondary:
        case PhysicalStateId::FootPlant:
        case PhysicalStateId::Boneless:
        case PhysicalStateId::HandPlant:
        case PhysicalStateId::RevertGround:
        case PhysicalStateId::KnownAir:
        case PhysicalStateId::BipedAir:
        case PhysicalStateId::BipedGround:
        case PhysicalStateId::OffBoardPushing:
        case PhysicalStateId::GroundAnimation:
        case PhysicalStateId::SlideGround:
        case PhysicalStateId::WipeoutGround:
        case PhysicalStateId::Teleporting:
        case PhysicalStateId::LandingOnDeck: return true;
        default: return false;
    }
}
}  // namespace

Status publish(GamePhysics& physics, SkaterRuntime& skater) {
    const PhysicalStateId state = skater.player_state.current();
    offboard::board_manager::runtime::publish(physics, skater);
    // 82DB7218..7258 runs for every state, before the selected state's Fill.
    // BipedAir may subsequently overwrite64 with its sampled trajectory velocity.
    {
        const auto& biped = skater.biped_ground.controller.state;
        skater.player_input.physical.off_board.vector_64 = rs::to_bits(biped.frame_output.velocity);
        skater.player_input.physical.off_board.cadence_phase_80 = biped.cadence.phase.phase;
        skater.player_input.physical.off_board.locomotion_state_84 = biped.cadence.locomotion_index;
    }
    if (!has_output_owner(state)) {
        return rs::make_err(fmt::cat("FillPhysOut requires the actual ", debug_name(state), " output owner"));
    }
    if (state == PhysicalStateId::KnownAir) S3G_CHECK(known_air::fill(physics, skater));
    if (state == PhysicalStateId::BipedAir) S3G_CHECK(biped_air::fill(skater));
    if (state == PhysicalStateId::LandingOnDeck) S3G_CHECK(landing_on_deck::fill(skater));
    if (state_ns::is_grind(state) || state == PhysicalStateId::Nonspecific) S3G_CHECK(grind::fill(physics, skater));
    std::optional<biped_ground::lifecycle::StatePublication> offboard_ground;
    if (state == PhysicalStateId::BipedGround || state == PhysicalStateId::OffBoardPushing) {
        S3G_TRY(auto out, biped_ground::fill(skater, skater.offboard_feet));
        offboard_ground = biped_ground::publish_fields(out, skater.player_input.physical);
    }
    const auto& p = skater.player_input.processed;
    S3G_TRY_OPT(const auto* toolkit, skater.player_input.toolkit ? &*skater.player_input.toolkit : nullptr,
                "State output requires actual board toolkit");
    std::optional<sc::riding::grounded::state::output::PhysicsGroundOutput> output;
    if (state == PhysicalStateId::PhysicsGround) output = skater.ground.output(p, skater.animation_input, *toolkit);
    auto& player = skater.player_input.player;
    skater.player_state.state_count = player.state_count_1312;
    skater.player_state.update_count = player.update_count_1316;
    auto& flags = skater.player_state.state_flags;
    // State template82DE3BE8 clears bytes52..87. Only source-owned writes follow.
    flags.fill(false);
    auto set = [&](std::size_t offset, bool value) { flags.at(offset - 52) = value; };
    set(52, (p.flags_2468 & (1u << 30)) != 0);
    set(54, (p.flags_2468 & (1u << 29)) != 0);
    set(55, (p.flags_2468 & (1u << 25)) != 0);
    set(56, (p.flags_2468 & (1u << 27)) != 0);
    set(57, (p.flags_2468 & (1u << 26)) != 0);
    set(58, (p.flags_2468 & (1u << 28)) != 0);
    set(59, (p.flags_2468 & (1u << 18)) != 0);
    set(60, skater.animation_input.fields.balance != 0.0f);
    set(62, (p.flags_2472 & (1u << 10)) != 0);
    set(63, state == PhysicalStateId::LandingOnDeck && skater.landing_on_deck.state.requests_board_flip());
    // State63 is set only by LandingOnDeckManager::Fill82D4E0E8..E0F4
    // when byte246 && time240<.02. Ordinary Ground never invokes that Fill,
    // so it retains the template zero here; State65 carries Ground requests.
    set(66, state == PhysicalStateId::RevertGround && skater.revert_state.active);
    set(65, skater.wipeout.requests_wipeout(p));
    set(78, skater.wipeout.requests_runout(p));
    set(71, (player.flags_1296 & (1u << 24)) != 0);
    set(73, (p.flags_2476 & (1u << 24)) != 0);
    set(74, state == PhysicalStateId::HandPlant && skater.handplant.continuation);
    set(75, state_ns::category(state) == 500);
    set(76, static_cast<std::uint32_t>(state) == 500);
    // 82DB78A8..78DC: the processed off-board request refreshes three
    // outputs, then this publication consumes one and exposes State77.
    if (p.state_identifier_2496 == 500) player.dismount_request_frames_1332 = 3;
    const bool dismount_requested = player.dismount_request_frames_1332 > 0;
    if (dismount_requested) player.dismount_request_frames_1332 -= 1;
    set(77, dismount_requested);
    set(79, p.state_variant_index_2528 == 3);
    set(80, (p.flags_2484 & (1u << 21)) != 0);
    set(83, skater.footplant.perform || skater.footplant.flag_627);
    if (output) {
        set(84, output->state_28.flag_84);
        set(86, output->state_28.has_world_grab_intent_without_object);
        if (output->state_28.manual_correction_write_78) set(78, *output->state_28.manual_correction_write_78);
    }
    set(87, (p.flags_2484 & (1u << 10)) != 0);
    if (offboard_ground) set(86, offboard_ground->flag_86);
    if (state == PhysicalStateId::SlideGround) set(84, skater.slide_state.state.wall_riding);
    {
        auto& physical = skater.player_input.physical;
        physical.component_1832_word_1876 = skater.handplant.flags;
        physical.air.handplant_position_304 = rs::to_bits(skater.handplant.anchor);
        physical.air.handplant_time_320 = skater.handplant.phase;
        physical.air.flag_446 = std::uint8_t((skater.handplant.flags & 0x8000'0000u) != 0);
        skater.footplant.publish(physical.air);
        // 82DB76E0..76F4 publishes this independently of selected-state304/311.
        physical.off_board.flag_308 = std::uint8_t((p.flags_2480 & (1u << 19)) != 0);
        // Common ProcessOutput82DB7044/7048 and70C0/70C4; these precede state Fill.
        const auto* selection = skater.trajectory.selector.selection();
        physical.air.flag_444 = std::uint8_t(selection != nullptr && selection->wall_ride);
        // 82DB6EC0 publishes 1596 and 1597 together as air 441 and 445.
        physical.air.flag_441 = std::uint8_t(skater.air_reckoning.state.flip_active);
        physical.air.flag_445 = std::uint8_t(skater.air_reckoning.state.flip_side);
        if (state == PhysicalStateId::GroundAnimation) skater.ground_animation.fill(p, physical.air);
        CurrentStateFields fields{};
        fields.category_12 = state_ns::category(state);
        fields.state_16 = static_cast<std::uint32_t>(state);
        fields.flag_66 = std::uint8_t(state == PhysicalStateId::RevertGround && skater.revert_state.active);
        fields.flag_74 = std::uint8_t(state == PhysicalStateId::HandPlant && skater.handplant.continuation);
        // ProcessOutput82DB7104..7128: selector57 requests State69 here.
        // The next input/selection enters702; never reset inside selection.
        fields.flag_69 = std::uint8_t(skater.player_state.selector.request_teleport);
        physical.state = fields;
        skater.player_state.state_flags[69 - 52] = physical.state.flag_69 != 0;
        if (offboard_ground) {
            physical.state.counter_36 = offboard_ground->word_36;
            physical.state.skitch_value_40 = offboard_ground->word_40;
        }
        if (output) {
            physical.state.skitch_value_40 = output->state_28.grab_spline_object_id;
            physical.state.signed_ground_step_84 = std::uint8_t(output->state_28.flag_84);
            physical.ground.vector_128 = rs::to_bits(output->ground_32.anti_flip_torque);
            physical.ground.scalar_276 = output->ground_32.time_to_skitch;
            physical.ground.flag_317 = std::uint8_t(output->ground_32.anti_flip_nudge_present);
            physical.ground.flag_318 = std::uint8_t(output->ground_32.is_pinning);
            physical.off_board.flag_304 = std::uint8_t(output->is_grabbing_object_72_304);
            physical.animation.manual_opposition_168 = std::uint8_t(output->manual_opposition_56_168);
        } else if (state == PhysicalStateId::PhysicsAir || state == PhysicalStateId::PhysicsAirSecondary) {
            const auto air = sc::air::state::fill_physics_output(skater.air_state);
            physical.air.landing_normal_144 = rs::to_bits(air.landing_normal);
            physical.air.jump_height_200 = air.jump_height;
            physical.air.reached_apex_436 = std::uint8_t(air.is_at_apex);
            if (air.scalar_184_write) physical.air.scalar_184 = *air.scalar_184_write;
        } else if (state == PhysicalStateId::SlideGround) {
            // State Fill82D3B010 is a single byte from this retained Slide owner.
            physical.state.signed_ground_step_84 = std::uint8_t(skater.slide_state.state.wall_riding);
        }
        // ProcessOutput82DB71C4 calls SkateboardController::FillPhysOut82D76D20
        // for every selected state. The constructor82D74DD8 seeds448=0; use the
        // same controller owner as state transitions and post-physics ragdoll.
        // Published by board_manager::runtime::publish above from retained controller state.

        // ProcessOutput82DB7130..7180 copies the HandPlantManager animation
        // flags1876 into Air304+20 one bit at a time, preserving the low28 bits.
        // The same manager word is consumed by the actual input publication.
        physical.air.handplant_flags_324 =
            (physical.air.handplant_flags_324 & 0x0fff'ffffu) | (physical.component_1832_word_1876 & 0xf000'0000u);

        if (output && output->velocity_projection_36) {
            const auto& projection = *output->velocity_projection_36;
            physical.reckoning.vector_144 = rs::to_bits(projection.velocity_without_axis_component);
            physical.reckoning.flag_164 = std::uint8_t(projection.active);
        }
    }
    std::array<bool, 4> contacts{};
    for (std::size_t i = 0; i < 4; ++i) contacts[i] = physics.riding.ground.parts[i].in_contact;
    const std::uint32_t surface = sc::physics::contact_feedback::choose_surface(
        physics.riding.wheel_lines.physics_surfaces, contacts, (physics.riding.ground.collision_flags & (1u << 25)) != 0);
    S3G_CHECK(grind::condition(physics, skater));
    auto& physical = skater.player_input.physical;
    FilteredStateInput input{};
    input.physics_category = static_cast<std::int32_t>(state_ns::category(state));
    input.physics_state = static_cast<std::int32_t>(state);
    input.anything_in_contact = physical.collision.flag_3477 != 0;
    input.physics_surface_type = static_cast<std::int32_t>(surface);
    input.wall_ride_exit = output.has_value() && output->ground_32.wall_ride_exit;
    // Original reset records; the selected ordinary Ground state and
    // authored empty-edge world do not publish the active air/grind fields.
    input.targeting_grind = state == PhysicalStateId::KnownAir && skater.known_air.state.targeting_grind_213;
    // 82DE6078..60C0: state501 is Offboard or OffboardAir from output320.
    input.offboard_has_landed = physical.off_board.flag_320 != 0;
    input.offboard_on_deck = physical.off_board.flag_315 != 0;
    if (state_ns::is_grind(state)) {
        S3G_TRY(input.grind, grind::Runtime::filtered_output(physical.grinds));
    } else {
        input.grind = GrindState{};
    }
    input.last_grind_distance = skater.grind.last_grind_distance();
    const auto filtered = skater.player_state.filtered.update(input);
    physical.filtered_state_0 = static_cast<std::uint32_t>(filtered.category);
    skater.player_state.filtered_output = filtered;
    skater.player_state.ground_output = output;
    if (state == PhysicalStateId::WipeoutGround) wipeout_output::publish(skater);
    if (state == PhysicalStateId::Teleporting) (void)skater.teleport_state.publish_output(skater.player_input.physical);
    return ok();
}

}  // namespace s3::game::physics::player_state::publication
