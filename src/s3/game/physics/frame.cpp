// Port of skate-game/src/physics/frame.rs.
#include "s3/game/physics/frame.hpp"

#include "s3/core/physics/board.hpp"
#include "s3/core/physics/phase.hpp"
#include "s3/game/camera.hpp"
#include "s3/game/graph_runtime.hpp"
#include "s3/game/physics.hpp"
#include "s3/game/physics/air_phase.hpp"
#include "s3/game/physics/animation_phase.hpp"
#include "s3/game/physics/biped_air.hpp"
#include "s3/game/physics/biped_ground.hpp"
#include "s3/game/physics/boneless.hpp"
#include "s3/game/physics/camera_output.hpp"
#include "s3/game/physics/climbing.hpp"
#include "s3/game/physics/climbing/approach.hpp"
#include "s3/game/physics/controls.hpp"
#include "s3/game/physics/foot_ik_queries.hpp"
#include "s3/game/physics/footplant/ground.hpp"
#include "s3/game/physics/grind.hpp"
#include "s3/game/physics/ground_animation.hpp"
#include "s3/game/physics/ground_phase.hpp"
#include "s3/game/physics/handplant.hpp"
#include "s3/game/physics/input_phase.hpp"
#include "s3/game/physics/known_air.hpp"
#include "s3/game/physics/landing_on_deck.hpp"
#include "s3/game/physics/offboard/board_manager/runtime.hpp"
#include "s3/game/physics/offboard/contact_toolkit.hpp"
#include "s3/game/physics/offboard_audit_trace.hpp"
#include "s3/game/physics/player_input.hpp"
#include "s3/game/physics/player_state.hpp"
#include "s3/game/physics/respawn.hpp"
#include "s3/game/physics/revert_state.hpp"
#include "s3/game/physics/riding_outputs.hpp"
#include "s3/game/physics/skater.hpp"
#include "s3/game/physics/slide_state.hpp"
#include "s3/game/physics/solve.hpp"
#include "s3/game/physics/wipeout_states.hpp"
#include "s3/game/state_names.hpp"
#include "s3/game/trainer.hpp"

namespace s3::game::physics::frame {

using sc::math::Vector3;
using sc::physics::board::BodyId;
using sc::physics::phase::PhysicalOutputSnapshot;
using sc::physics::phase::PhysicsEvent;
using sc::player::state::PhysicalStateId;

Status advance(GamePhysics& physics, SkaterRuntime& skater, PlayerControls& controls,
               const graph_runtime::StockGraphs& graphs, sc::input::controller::ActionMap& actions,
               bool input_available, camera::CameraRuntime& camera) {
    const std::uint64_t tick = physics.ticks;
    offboard_audit_trace::stage(tick, "begin", physics, skater, controls);
    physics.exchange = SimulationExchange::new_(tick);
    //SimController8285C968 dispatches the preceding tick's camera messages
    //before simulation. Keep End/Begin ordering when both occur in one update.
    {
        auto requests = std::move(camera.simulation_rate_requests);
        camera.simulation_rate_requests.clear();
        for (const auto& request : requests) {
            if (!physics.network_active) {
                S3G_CHECK(physics.clock.apply(request));
            }
        }
    }
    if (physics.network_active) {
        physics.clock = clock::SimulationClock{};
    }
    if (physics.ticks == 0) {
        S3G_CHECK(player_state::initialize(physics, skater));
        //Ctor82DB3008 enters Ground without ProcessOutput. Keep the constructed
        //State/FilteredState zeros (82DE3BE8/82DE5588) until the completed tick.
        //Publishing Ground here runs its ForcePhysics Begin before the initial
        //input reset82DB8998, which would immediately clear that mode again.
    }
    {
        S3G_TRY(const bool climbed, climbing::advance(physics, skater, controls, camera));
        if (climbed) return ok();
    }
    //8285A06C/A080 completes prior trajectory batches, then A0D0 calls
    //Player slot16=82DB3C78 (vtable82328688). Consume with retained input
    //BEFORE this frame's queries, ProcessInput, and state selection.
    S3G_CHECK(biped_air::consume_selector(physics, skater));
    physics.board.clear_forces();
    //World8275EA20 starts both batches before actor SetUpPhysics. Foot
    //results stay pending while PlayerInput consumes the preceding records.
    S3G_CHECK(physics.riding.start_wheel_queries(physics.board, physics.world));
    S3G_TRY(auto skeleton_queries, foot_ik_queries::query(physics.world, skater.skeleton));
    auto animation_result =
        animation_phase::advance(physics, skater, controls, graphs, physics.animation_profile);
    if (animation_result.is_err()) {
        return rs::make_err(fmt::cat("Animation tick", physics.ticks, ": ", animation_result.unwrap_err()));
    }
    auto animation = std::move(animation_result.unwrap());
    offboard_audit_trace::stage(tick, "animation", physics, skater, controls);
    //ForcePhysics Begin82BB2868 writes the live Skeleton16420 mode once.
    //Consume the graph request so a later physical reset can retain its own0.
    {
        auto& force_mode = skater.animation.motion.riding.force_mode;
        if (force_mode.has_value()) {
            const auto mode = *force_mode;
            force_mode.reset();
            skater.skeleton_input.force_mode = native_value(mode);
        }
    }
    //PlayerUI82898920 remaps the gameplay packet before both controller
    //and physical input consume it. Reuse this tick's sampled mapping.
    auto simulation_actions = controls.simulation_actions(actions);
    S3G_TRY(const bool teleported,
            input_phase::advance(physics, skater, animation.packet(), simulation_actions, input_available));
    S3G_TRY(skater.ground_settings,
            skater.ground_profiles.select(skater.player_input.processed.state_variant_index_2528,
                                          skater.player_input.processed.surface_mode_2540));
    if (physics.trainer != trainer::TrainerTuning{}) {
        skater.ground_settings =
            std::make_shared<const ground_runtime::GroundSettings>(skater.ground_settings->tuned(physics.trainer));
    }
    bool vehicle_ejected = false;
    if (teleported) {
        skater.respawn.reset_measurements();
        //82DB93B0..CC: complete pending queries and clear contact history.
        skater.offboard_contact.reset_history();
        skater.player_state.reset_for_teleport();
        skater.centre_of_mass_filter.reset();
        skater.animation_feedback.reset();
        S3G_CHECK(player_state::enter_after_teleport(physics, skater));
        offboard::board_manager::runtime::finish_teleport(physics, skater);
        //82DB8E40 calls SetUpNormal after pose reset, state100 and board reset.
        skater.wipeout_state.ragdoll.restore_normal(skater.skeleton, skater.skeleton_joints,
                                                    skater.skeleton_collision, skater.collision_feedback);
        S3G_TRY(vehicle_ejected, player_state::apply_vehicle_ejection(physics, skater));
    }
    //World8275EC0C ends board queries before PostInput/state selection.
    S3G_CHECK(physics.riding.finish_wheel_queries());
    //Complete preceding-state submissions before PostInput82DB573C publishes
    //them. Host execution is synchronous; no current-state submission exists
    //yet, preserving next-tick visibility and later PreState82DB60EC consumption.
    {
        const auto scene = offboard::grab_scene::Scene::new_(physics.world, physics.offboard_grab_scene);
        S3G_CHECK(skater.offboard_grab.execute_queries(scene));
    }
    const PhysicalStateId state_before_selection = skater.player_state.current();
    if (vehicle_ejected) {
        // Vehicle ejection is a host transition that must retain WipeoutGround,
        // so it bypasses the native state selector. ProcessInput has already
        // prepared this tick's grind work, however, and native PostInput is its
        // mandatory consumer. Skipping both phases leaves a stale request that
        // aborts the following tick.
        S3G_CHECK(player_state::complete_post_input(physics, skater));
    } else {
        S3G_CHECK(player_state::post_input_and_select(physics, skater));
    }
    const PhysicalStateId state_after_selection = skater.player_state.current();
    offboard_audit_trace::stage(tick, "selected", physics, skater, controls);
    if (state_before_selection != state_after_selection) {
        if ((skater.player_input.processed.flags_2476 & (1u << 22)) != 0 ||
            state_before_selection == PhysicalStateId::HandPlant ||
            state_after_selection == PhysicalStateId::HandPlant) {
            char buf[256];
            std::snprintf(buf, sizeof buf,
                          "HANDPLANT_STATE tick=%llu from=%s to=%s flags=%08x phase=%u processed=%08x/%08x/%08x",
                          static_cast<unsigned long long>(tick), debug_name(state_before_selection),
                          debug_name(state_after_selection), unsigned(skater.handplant.flags),
                          unsigned(skater.handplant.phase), unsigned(skater.player_input.processed.flags_2468),
                          unsigned(skater.player_input.processed.flags_2476),
                          unsigned(skater.player_input.processed.flags_2480));
            log::info(buf);
        }
        S3G_CHECK(physics.exchange.emit_event(
            tick, PhysicsEvent{PhysicsEvent::StateChanged{state_before_selection, state_after_selection}}));
    }
    S3G_CHECK(player_state::pre_state(physics, skater));
    const PhysicalStateId current = skater.player_state.current();
    switch (current) {
        case PhysicalStateId::RevertGround:
            S3G_CHECK(revert_state::update(physics, skater));
            break;
        case PhysicalStateId::HandPlant:
            S3G_CHECK(handplant::update(physics, skater));
            break;
        case PhysicalStateId::FootPlant:
            S3G_CHECK(footplant::ground::update(physics, skater));
            break;
        case PhysicalStateId::Boneless:
            S3G_CHECK(boneless::update(physics, skater));
            break;
        case PhysicalStateId::PhysicsGround: {
            const auto& p = skater.player_input.processed;
            const auto com = rs::from_bits(p.animation_com_to_deck_752);
            riding_outputs::RidingPoseInputs pose{};
            pose.com_to_deck = Vector3::new_(com[0], com[1], com[2]);
            pose.body_spin = skater.animation_input.extra.physical_body_spin;
            physics.riding.update_ground_reckoning(physics.board, pose, p.flags_2468,
                                                   skater.animation_input.fields.balance,
                                                   (p.flags_2476 & 0x40000000u) != 0, p);
            //PhysicsGround::PreUpdate82D37C50 clears the previous push suppression
            //after Reckoning; this frame's propulsion may then set it again.
            skater.ground.state.push_suppressed_2730 = false;
            S3G_CHECK(ground_phase::advance(physics, skater));
            S3G_CHECK(handplant::ground_update(physics, skater));
            S3G_CHECK(input_phase::update_ground(physics, skater));
            break;
        }
        case PhysicalStateId::PhysicsAir:
        case PhysicalStateId::PhysicsAirSecondary:
            S3G_CHECK(air_phase::advance(physics, skater));
            break;
        case PhysicalStateId::KnownAir:
            S3G_CHECK(known_air::update(physics, skater));
            break;
        case PhysicalStateId::BipedAir:
            S3G_CHECK(biped_air::update::update(physics, skater, controls));
            break;
        case PhysicalStateId::BipedGround:
        case PhysicalStateId::OffBoardPushing: {
            sc::player::offboard::ground_job::ContactSnapshot contact{};
            contact.readiness = static_cast<std::int32_t>(skater.offboard_contact.readiness());
            contact.prefix = skater.offboard_contact.prefix();
            S3G_TRY(auto input, biped_ground::update(physics, skater, controls, contact));
            //Sync82D32164 submits AFTER Skeleton; both query batches become
            //visible only to their next input/PreUpdate consumer.
            S3G_TRY(auto scene, offboard::contact_toolkit::StaticScene::new_(physics.world));
            S3G_CHECK(skater.offboard_contact.submit(
                input, static_cast<std::int32_t>(skater.player_input.processed.actor_query_2952), scene));
            S3G_CHECK(biped_ground::submit_geometry(physics, skater));
            break;
        }
        case PhysicalStateId::GroundAnimation:
            S3G_CHECK(ground_animation::advance(physics, skater));
            break;
        case PhysicalStateId::LandingOnDeck:
            S3G_CHECK(landing_on_deck::advance(physics, skater));
            break;
        case PhysicalStateId::SlideGround:
            S3G_CHECK(slide_state::update(physics, skater));
            break;
        case PhysicalStateId::WipeoutGround:
            S3G_CHECK(wipeout_states::advance(physics, skater));
            break;
        case PhysicalStateId::Teleporting:
            //VT823272FC slot8=82D431F0; slots12/40 are original empty leaves.
            if (skater.teleport_state.update(skater.player_input.processed)) {
                S3G_CHECK(respawn::request(physics, skater));
            }
            break;
        default:
            if (sc::player::state::is_grind(current) || current == PhysicalStateId::Nonspecific) {
                S3G_CHECK(grind::advance(physics, skater));
            } else {
                return rs::make_err(fmt::cat("Selected ", debug_name(current), " requires its physical update"));
            }
            break;
    }
    // Source State82DB6120 submits the queue once, then advances state time.
    // BoardRuntime applies that same queue during its shared solve.
    skater.player_input.player.state_timer_1344 += skater.player_input.processed.timestep_2604;
    offboard_audit_trace::stage(tick, "state", physics, skater, controls);
    //82DB6150: update the real possession owner after state movement, before solve.
    offboard::board_manager::runtime::update(physics, skater);
    offboard_audit_trace::stage(tick, "possession", physics, skater, controls);
    physics.processed_flags_2468 = skater.player_input.processed.flags_2468;
    //World8275ECA4 ends skeleton tests after state/forces and before solving.
    //Teleport resets previous observations, but preserves this pending batch.
    skeleton_queries.publish(skater.player_input.player);
    S3G_CHECK(solve::advance(physics, skater, skater.ground.steering.targets));
    offboard_audit_trace::stage(tick, "solve", physics, skater, controls);
    //ProcessOutput82DB6EE8 resets the packet before its component publishers.
    //All consumers of the preceding output have completed this frame's input.
    player_input::reset_outputs(skater.player_input.physical);
    S3G_CHECK(physics.finish_skater(skater));
    offboard_audit_trace::stage(tick, "finish", physics, skater, controls);
    const auto simulation = physics.settings.step.simulation;
    skater.player_input.update_dynamic_normal(physics.riding, simulation.gravity_acceleration);
    S3G_CHECK(skater.player_input.publish_board(physics.riding));
    skater.player_input.physical.skeleton.publish_deck_angles(skater.animated_skeleton.record.pose[0][2],
                                                              skater.animation.packet.board_flipped);
    const auto up = physics.riding.reckoning.up;
    S3G_CHECK(skater.player_input.publish_grind_graph_outputs(skater.skeleton.record, {up.x, up.y, up.z, 0.0f},
                                                              skater.trajectory.selector));
    S3G_CHECK(player_state::publish(physics, skater));
    skater.grind_camera.condition_fields(skater.player_input.physical.grinds);
    {
        const auto& physical = skater.player_input.physical;
        skater.centre_of_mass_output = skater.centre_of_mass_filter.update(
            rs::from_bits(physical.reckoning.vector_64), rs::from_bits(physical.reckoning.vector_16));
    }
    const auto feedback = animation_phase::publish_feedback(physics, skater);
    S3G_CHECK(respawn::observe(physics, skater));
    const auto deck = physics.board.bodies()[sc::physics::board::index(BodyId::Deck)];
    const auto& rider = skater.skeleton.bodies()[0];
    const auto ground_normal = physics.riding.ground.wheel_normal;
    const auto predicted = skater.player_input.physical.collision.predicted_position_64;
    const Vector3 predicted_position =
        Vector3::new_(rs::from_bits(predicted[0]), rs::from_bits(predicted[1]), rs::from_bits(predicted[2]));
    const auto events = physics.exchange.events();
    PhysicalOutputSnapshot snapshot{};
    snapshot.tick = tick;
    snapshot.state = skater.player_state.current();
    snapshot.board_position = deck.rates.position;
    snapshot.board_linear_velocity = deck.rates.linear_velocity;
    snapshot.rider_root_position = rider.rates.position;
    snapshot.rider_linear_velocity = rider.rates.linear_velocity;
    snapshot.ground_normal = ground_normal;
    snapshot.contact_count = static_cast<std::uint32_t>(physics.riding.ground.part_contact_count);
    snapshot.predicted_position = predicted_position;
    snapshot.grounded = skater.player_state.current() == PhysicalStateId::PhysicsGround;
    snapshot.wiping_out = skater.skeleton_collision.is_ragdoll;
    bool landed = false;
    for (const auto& event : events) landed = landed || std::holds_alternative<PhysicsEvent::Landing>(event.value);
    snapshot.landed = landed;
    snapshot.footstep_strength = skater.animation_input.extra.footstep_strength;
    snapshot.footstep_bone = skater.animation_input.contacts.bone;
    snapshot.foot_push_speed = skater.animation_input.contacts.push_speed;
    for (std::size_t i = 0; i < snapshot.feet_supported.size(); ++i) {
        snapshot.feet_supported[i] = skater.offboard_feet.hands[i].flags_104_to_107[1];
    }
    snapshot.events = events;
    physics.exchange.publish_output(std::move(snapshot));
    S3G_CHECK(camera_output::advance(physics, skater, feedback, camera));
    const auto& score = skater.animation.motion.score_packet;
    const auto deck_frame = physics.board.part_transforms()[sc::physics::board::index(BodyId::Deck)];
    const auto position = deck.rates.position;
    const auto velocity = deck.rates.linear_velocity;
    const auto filtered = skater.player_state.filtered_output;
    scoring_runtime::Frame f{};
    f.tick = static_cast<std::uint32_t>(tick);
    f.dt = simulation.time_step;
    f.category = filtered ? filtered->category : decltype(f.category){};
    f.state = static_cast<std::uint32_t>(skater.player_state.current());
    // `ScoringHandPlants` (82BBF758) selects a handplant's name the same way
    // `ScoringGrabs` (82BBEF60) selects a grab's (see Rust comment).
    if (score.trick_names.first.has_value()) {
        f.descriptor = score.trick_names.first;
    } else if (score.grab.has_value()) {
        f.descriptor = score.grab->first;
    } else if (score.handplant.has_value()) {
        f.descriptor = score.handplant->first;
    }
    f.grind_id = filtered ? filtered->grind.scorable_id : -1;
    f.flags = score.flags;
    f.position = {position.x, position.y, position.z};
    f.velocity = {velocity.x, velocity.y, velocity.z};
    f.forward = deck_frame.basis.columns[2];
    // PhysOut_Skeleton +368, which 82BE1AE8 fills from Physics::Skeleton +11920 -- the
    // rider's animation-to-world root. Rows are right / up / forward / translation.
    {
        const auto& row1 = skater.player_input.physical.skeleton.anim_to_world_11920[1];
        const auto& row2 = skater.player_input.physical.skeleton.anim_to_world_11920[2];
        for (std::size_t lane = 0; lane < 3; ++lane) {
            f.rider_up[lane] = rs::from_bits(row1[lane]);
            f.rider_forward[lane] = rs::from_bits(row2[lane]);
        }
    }
    f.switch_ = skater.animation.packet.riding_switch;
    f.fakie = skater.animation.packet.riding_fakie;
    f.nollie = skater.animation.packet.weight_forwards;
    f.body_flip = skater.player_input.physical.air.flag_441 != 0;
    f.body_flip_side = skater.player_input.physical.air.flag_445 != 0;
    // PhysOut.Ground +192 and +208 (see Rust comment).
    {
        const auto& origin = skater.player_input.player.hips_line_origin_1632;
        for (std::size_t lane = 0; lane < 3; ++lane) f.hips_position[lane] = rs::from_bits(origin[lane]);
        const auto& hips = skater.player_input.player.hips_line_test_1488;
        if (hips.valid != 0) {
            std::array<float, 3> g{};
            for (std::size_t lane = 0; lane < 3; ++lane) g[lane] = rs::from_bits(hips.position[lane]);
            f.hips_ground = g;
        }
        f.hips_surface = hips.surface;
    }
    f.suspend_air = skater.player_input.physical.air.use_air_reckoning_452 != 0;
    f.landing = skater.landing_quality;
    f.teleported = teleported;
    // Revert Fill (82D43B10) publishes its active lifetime in State66.
    f.reverting = skater.player_input.physical.state.flag_66 != 0;
    // State70 is a different byte (see Rust comment); it stays clear.
    f.manual_block_70 = false;
    S3G_CHECK(skater.scoring.advance(f));
    climbing::approach::advance(physics, skater, controls);
    skater.animation_input.finish_output_publication();
    skater.player_input.player.update_count_1316 = skater.player_input.player.update_count_1316 + 1u;
    physics.clock.finish_tick();
    return ok();
}

}  // namespace s3::game::physics::frame
