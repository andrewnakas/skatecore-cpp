// Port of skate-game/src/physics/player_state/transition.rs.
//! Native SetPhysicsState82DB8540 publication, followed by owned Enter/Exit.
#include "s3/game/physics/player_state/transition.hpp"

#include "s3/core/physics/board_toolkit.hpp"
#include "s3/core/player/lifecycle.hpp"
#include "s3/game/crash_context.hpp"
#include "s3/game/physics.hpp"
#include "s3/game/physics/air_phase.hpp"
#include "s3/game/physics/biped_air.hpp"
#include "s3/game/physics/biped_ground.hpp"
#include "s3/game/physics/boneless.hpp"
#include "s3/game/physics/footplant.hpp"
#include "s3/game/physics/footplant/ground.hpp"
#include "s3/game/physics/grind.hpp"
#include "s3/game/physics/ground_animation.hpp"
#include "s3/game/physics/ground_exit.hpp"
#include "s3/game/physics/ground_phase.hpp"
#include "s3/game/physics/handplant.hpp"
#include "s3/game/physics/known_air.hpp"
#include "s3/game/physics/landing_on_deck.hpp"
#include "s3/game/physics/offboard/board_manager.hpp"
#include "s3/game/physics/offboard/board_manager/runtime.hpp"
#include "s3/game/physics/revert_state.hpp"
#include "s3/game/physics/skater.hpp"
#include "s3/game/physics/slide_state.hpp"
#include "s3/game/physics/wipeout_states.hpp"
#include "s3/game/state_names.hpp"

namespace s3::game::physics::player_state::transition {

using namespace sc::player::lifecycle;
using sc::player::state::PhysicalStateId;
namespace state = sc::player::state;

namespace {

bool supported_exit(PhysicalStateId s) {
    if (state::is_grind(s) || s == PhysicalStateId::Nonspecific) return true;
    switch (s) {
        case PhysicalStateId::Sleeping:
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

struct Calls final : PhysicalStateCalls {
    PhysicalStateId get_type(StateBinding s) override { return s.state; }
    // Host lifecycle publication is completed below before applying physical
    // effects. These supported Exit methods do not inspect the active binding.
    void exit(StateCall call) override { S3_ASSERT(supported_exit(call.state.state)); }
    void enter(StateCall call) override {
        S3_ASSERT(call.state.state != PhysicalStateId::Sleeping && supported_exit(call.state.state));
    }
};

}  // namespace

Status set(GamePhysics& physics, SkaterRuntime& skater, PhysicalStateId requested) {
    // RevertGround owns a real native Enter/Update/Fill lifecycle. Preserve
    // the selector request so its turning controller can run.
    skater.player_state.requested_state = requested;
    const PhysicalStateId current = skater.player_state.current();
    if (current == requested) return ok();
    crash_context::requested(physics.ticks, current, requested);
    const auto board_observation = offboard::board_manager::runtime::observe(physics, skater);
    auto& p = skater.player_input.processed;
    log::info(fmt::cat("physical state transition current=", debug_name(current), " requested=", debug_name(requested),
                       " board_controller_state=", skater.skateboard_controller.fields.state_448,
                       " board_controller_on=", skater.skateboard_controller.fields.system_on_452,
                       " processed_state=", p.state_2508, " processed_category=", p.category_2512));
    // Keep the actual selector request for the coordinator; do not publish a
    // state whose physical Enter/Exit owner has not been connected.
    if (!skater.player_state.registry.can_transition(current, requested)) {
        return rs::make_err(fmt::cat("Physical state transition ", debug_name(current), " -> ", debug_name(requested),
                                     " requires its native Enter/Exit production adapter"));
    }
    auto& player = skater.player_input.player;
    StateChangeData data{};
    data.player.word_1312 = player.state_count_1312;
    data.player.previous_category_latch_1336 = player.state_value_1336;
    data.player.scalar_1344 = player.state_timer_1344;
    data.processed.flags_2480 = p.flags_2480;
    data.processed.requested_state_2500 = static_cast<std::uint32_t>(requested);
    data.processed.previous_state_2504 = p.state_2504;
    data.processed.current_state_2508 = p.state_2508;
    data.processed.current_category_2512 = p.category_2512;
    data.processed.previous_category_2516 = p.category_2516;
    data.processed.previous_category_latch_2520 = p.player_state_value_2520;
    data.processed.word_2564 = p.state_count_2564;
    data.processed.scalar_2664 = p.state_timer_2664;
    data.skateboard_controller = skater.skateboard_controller.fields;
    auto effects =
        skater.board_possession_live.effects(physics, skater.ground_lifecycle.board_animated_290, p.timestep_2604);
    auto controller_actions = offboard::board_manager::Transition::new_(skater.board_possession, board_observation,
                                                                       effects, data.skateboard_controller);
    Calls calls;
    const auto changed = skater.player_state.lifecycle.set_physics_state(static_cast<std::uint32_t>(requested), data,
                                                                         calls, controller_actions);
    if (changed.is_err()) return rs::make_err(fmt::cat("Unknown physical state ", changed.unwrap_err().value));
    std::move(controller_actions).finish(data.skateboard_controller);
    skater.board_possession_live.publish_volumes(physics);
    player.state_count_1312 = data.player.word_1312;
    player.state_value_1336 = data.player.previous_category_latch_1336;
    player.state_timer_1344 = data.player.scalar_1344;
    p.flags_2480 = data.processed.flags_2480;
    p.state_2504 = data.processed.previous_state_2504;
    p.state_2508 = data.processed.current_state_2508;
    p.category_2512 = data.processed.current_category_2512;
    p.category_2516 = data.processed.previous_category_2516;
    p.player_state_value_2520 = data.processed.previous_category_latch_2520;
    p.state_count_2564 = data.processed.word_2564;
    p.state_timer_2664 = data.processed.scalar_2664;
    skater.skateboard_controller.fields = data.skateboard_controller;
    if (!skater.player_input.toolkit.has_value()) {
        skater.player_input.toolkit = sc::physics::board_toolkit::BoardToolkit::from_board(
            physics.board, p.flags_2468, p.scalar_2612, rs::from_bits(p.vectors_464_480_496_512_528[0]),
            {0.0f, 1.0f, 0.0f, 0.0f});
    }
    // Native82DB8540 publishes Processed state/history BEFORE old Exit. The
    // same retained objects then receive Exit followed by the new Enter.
    switch (current) {
        case PhysicalStateId::RevertGround:
            // Native Exit is empty; this is diagnostic-only host reporting.
            log::info(fmt::cat("REVERT_EXIT tick=", physics.ticks, " requested=", debug_name(requested)));
            break;
        case PhysicalStateId::HandPlant: skater.handplant.reset(); break;
        case PhysicalStateId::FootPlant: skater.footplant.reset(); break;  // Exit82D4C5A8
        case PhysicalStateId::Boneless: break;                              // empty82D4C9B4
        case PhysicalStateId::PhysicsGround: ground_exit::exit(physics, skater); break;
        case PhysicalStateId::PhysicsAir:
        case PhysicalStateId::PhysicsAirSecondary: air_phase::exit(skater); break;
        case PhysicalStateId::KnownAir:
            S3G_CHECK(known_air::exit(physics, skater, static_cast<std::uint32_t>(requested)));
            break;
        case PhysicalStateId::BipedAir: biped_air::exit(skater); break;
        case PhysicalStateId::BipedGround: biped_ground::exit(skater); break;
        case PhysicalStateId::OffBoardPushing: biped_ground::exit(skater); break;
        case PhysicalStateId::GroundAnimation: S3G_CHECK(ground_animation::exit(physics, skater)); break;
        case PhysicalStateId::LandingOnDeck: S3G_CHECK(landing_on_deck::exit(physics, skater)); break;
        case PhysicalStateId::SlideGround: S3G_CHECK(slide_state::exit(physics, skater)); break;
        case PhysicalStateId::WipeoutGround: wipeout_states::exit(physics, skater); break;
        case PhysicalStateId::Sleeping:
        case PhysicalStateId::Teleporting: break;  // 82B61BB8.
        default:
            if (state::is_grind(current) || current == PhysicalStateId::Nonspecific) {
                S3G_CHECK(grind::exit(physics, skater));
            } else {
                S3_PANIC("internal error: entered unreachable code: state support checked before publication");
            }
            break;
    }
    Status entered = ok();
    switch (requested) {
        case PhysicalStateId::RevertGround: entered = revert_state::enter(physics, skater); break;
        case PhysicalStateId::HandPlant: entered = handplant::enter(physics, skater); break;
        case PhysicalStateId::FootPlant: entered = footplant::ground::enter(physics, skater); break;
        case PhysicalStateId::Boneless: entered = boneless::enter(physics, skater); break;
        case PhysicalStateId::PhysicsGround: entered = ground_phase::enter(physics, skater); break;
        // PhysicsAirSecondary 202 runs the recovered PhysicsAir200 lifecycle.
        //
        // INFERRED, not recovered: no vtable or method address for the object at
        // PhysicalPlayer offset 1716 appears anywhere in this port, because the S2 debug
        // build that named the rest has no counterpart for it -- 202 is an S3 addition, like
        // the sixth (darkslide) grind family grind_chromosome.rs records. Five checks support
        // sharing 200's lifecycle rather than inventing one:
        //
        // 1. Owner offsets are adjacent, 1712 and 1716 (state.rs native_owner_offset), which
        //    is how a second instance of one class appears, not a different class.
        // 2. air_phase::enter already branches on previous_physics_category_2516 == 400, the
        //    grind family, so entry to air out of a grind is an already-recovered path.
        // 3. Both 200 and GroundAnimation103 enter through enable_angular_only, so this
        //    lifecycle does not fight the FORCE_ANIM_SKATEBOARD the authored DarkSlideTrick
        //    state sets -- the crash that motivated this recorded force_mode=2, that mode.
        // 4. selector/mod.rs treats 202 as transient: it leaves for 200 the moment
        //    flags_2484 bit 20 clears, and for WipeoutGround on a wipeout. Same family.
        // 5. 202 is only ever entered while the board is animation-driven, so the solver
        //    difference between the two objects, if any, has nothing to act on.
        //
        // What would disprove it: 202's native Update or Fill writing different PhysOut
        // fields than 200's. That shows up as wrong air metrics or scoring on darkslide
        // exits, never as a crash. See docs/engine-defects.md #10.
        case PhysicalStateId::PhysicsAir:
        case PhysicalStateId::PhysicsAirSecondary: entered = air_phase::enter(physics, skater); break;
        case PhysicalStateId::KnownAir: entered = known_air::enter(physics, skater); break;
        case PhysicalStateId::BipedAir: entered = biped_air::enter(physics, skater); break;
        case PhysicalStateId::BipedGround: entered = biped_ground::enter(physics, skater); break;
        case PhysicalStateId::OffBoardPushing: entered = biped_ground::enter(physics, skater); break;
        case PhysicalStateId::GroundAnimation: entered = ground_animation::enter(physics, skater); break;
        case PhysicalStateId::LandingOnDeck: entered = landing_on_deck::enter(physics, skater); break;
        case PhysicalStateId::SlideGround: entered = slide_state::enter(physics, skater); break;
        case PhysicalStateId::WipeoutGround: entered = wipeout_states::enter(physics, skater); break;
        case PhysicalStateId::Teleporting:
            skater.teleport_state.enter();
            entered = ok();
            break;
        default:
            if (state::is_grind(requested) || requested == PhysicalStateId::Nonspecific) {
                entered = grind::enter(physics, skater);
            } else {
                S3_PANIC("internal error: entered unreachable code: state support checked before publication");
            }
            break;
    }
    S3G_CHECK(entered);
    // Keep the command stream alongside the native lifecycle publication.
    // The direct calls above remain the owner of TU3 Enter/Exit ordering;
    // this record is the coordinator-facing audit/consumer boundary.
    S3G_CHECK(physics.exchange.request_state(requested));
    return ok();
}

}  // namespace s3::game::physics::player_state::transition
