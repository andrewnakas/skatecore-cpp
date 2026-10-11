// Port of skate-game/src/physics/animation_phase.rs.
//! The production physical-output -> stock graph -> next physical-input phase.
//! Native actor phases82592FD8/82593128/82593230/82593640 preserve this order.
#include "s3/game/physics/animation_phase.hpp"

#include "s3/core/animation/air_leg_extension.hpp"
#include "s3/core/animation/body_tilt.hpp"
#include "s3/core/animation/ground_acceleration.hpp"
#include "s3/core/animation/landing_quality.hpp"
#include "s3/core/animation/riding_fakie.hpp"
#include "s3/core/graph/conditions.hpp"
#include "s3/core/input/grind_intentions.hpp"
#include "s3/core/input/offboard_intentions.hpp"
#include "s3/core/physics/board.hpp"
#include "s3/core/player/conditioner_capabilities.hpp"
#include "s3/core/riding/push_behaviors.hpp"
#include "s3/game/graph_runtime.hpp"
#include "s3/game/physics.hpp"
#include "s3/game/physics/animation_grind.hpp"
#include "s3/game/physics/controls.hpp"
#include "s3/game/physics/skater.hpp"
#include "s3/game/physics/solve.hpp"
#include "s3/game/skater_animation.hpp"

namespace s3::game::physics::animation_phase {

namespace grind = animation_grind;
using sc::animation::physical_feedback::ControlFeedback;
using sc::animation::physical_feedback::PhysicalFeedback;
using sc::animation::physical_feedback::ReckoningFeedback;
using sc::graph::conditions::ConditionInputs;
using sc::graph::conditions::PushBrakeInputs;
using sc::math::Vector3;
using sc::physics::board::BodyId;
using sc::riding::push_behaviors::PushFootFrame;
using skater_animation::AnimationPhysical;

namespace {
std::size_t deck_index() { return sc::physics::board::index(BodyId::Deck); }
std::array<float, 4> f4(const std::array<std::uint32_t, 4>& raw) { return rs::from_bits(raw); }
}  // namespace

rs::Result<AnimationPhaseOutput, std::string> advance(const GamePhysics& physics, SkaterRuntime& skater,
                                                      const PlayerControls& controls,
                                                      const graph_runtime::StockGraphs& graphs,
                                                      const AnimationProfile& profile) {
    auto& motion = skater.animation.motion;
    motion.score_packet = {};
    const auto& p = skater.player_input.processed;
    const auto& physical = skater.player_input.physical;
    motion.manual_exit = physical.animation.manual_opposition_168 != 0;
    motion.deck_yaw_pitch = std::array<float, 2>{physical.skeleton.deck_yaw_536, physical.skeleton.deck_pitch_540};
    // ProcessOutput82DB7250/7258 publishes shared Biped720/716 as
    // OffBoard80/84. BipedCadence/MatchCadence/LocoState consume that completed
    // physical packet, preserving the publication boundary across transitions.
    // The native publication is unconditional, including on-board frames
    // when a dismount node can capture MatchCadence before state500 is entered.
    motion.offboard_cadence_phase = physical.off_board.cadence_phase_80;
    motion.offboard_locomotion_state = physical.off_board.locomotion_state_84;
    // GroundSlopeType82BA7E80 and IsBipedGroundThin82BA8110 read the
    // completed OffBoard88/330 directly, without a state/category gate.
    motion.ground_slope_type = physical.off_board.kind_88;
    motion.biped_ground_thin = physical.off_board.flag_330 != 0;
    skater.animation.action.dropping_in = physical.grinds.dropping_in_324 != 0;
    const auto deck = physics.board.part_transforms()[deck_index()];
    const auto [fakie, mirrored] = skater.animation.stance();
    // ToggleBoard consumes the completed FillPhysOut publication from the
    // preceding tick. Keep this input explicit so unsupported behavior cannot
    // silently run against fabricated board state.
    {
        graph_host::motion_toggle_board::Physical t;
        t.grabbing_object = physical.off_board.flag_304 != 0;
        t.holding_board = physical.off_board.flag_311 != 0;
        t.free_board = physical.off_board.free_board_312 != 0;
        t.retrieval_blocked = skater.player_state.state_flags[87 - 52];
        // ToggleBoard82BA968C consumes returning313, not retrieval323.
        t.retrieval_active = physical.off_board.returning_board_313 != 0;
        t.yaw_radians = physical.off_board.angle_36;
        t.pitch_radians = physical.off_board.angle_40;
        motion.toggle_board_physical = t;
    }
    {
        graph_host::motion_runout::Observation o;
        o.offboard_flag_331 = physical.off_board.trajectory_valid_331 != 0;
        o.offboard_velocity_128 = f4(physical.off_board.vector_64);
        o.reckoning_velocity_16 = f4(physical.reckoning.vector_16);
        o.reckoning_up_96 = f4(physical.reckoning.vector_96);
        o.skeleton_vector_0 = skater.skeleton_input.drive_frames[0][0];
        o.animation_mirrored = mirrored;
        motion.runout_physical = o;
    }
    const PhysicalFeedback feedback = skater.physical_feedback;
    {
        graph_host::motion_wipeout::Physical w;
        w.over_599 = physical.skeleton.over_599 != 0;
        w.collision_time_144 = physical.animation.collision_time_144;
        w.no_support_time_548 = physical.skeleton.no_support_time_548;
        w.profile_148 = physical.animation.profile_148;
        w.below_surface_82 = skater.player_state.state_flags[82 - 52];
        // Skeleton Fill82BE1E94/98 copies hips physical up into output80.
        w.orientation_y = skater.skeleton.record.pose[23][1][1];
        w.hips_right_angle_496 = physical.skeleton.hips_right_angle_496;
        w.hips_up_angle_500 = physical.skeleton.hips_up_angle_500;
        motion.wipeout_physical = w;
    }
    {
        graph_host::motion_landing::Physical l;
        l.height = feedback.crouching.animation_height_72;
        l.spin = skater.landing_quality.spin_92;
        l.kind = skater.landing_quality.landing_type_96;
        l.last_good_landing_velocity = motion.riding.last_good_landing_velocity;
        motion.landing_physical = l;
    }
    {
        graph_host::motion_spin::PrelandingPhysical pl;
        pl.air_444 = physical.air.flag_444 != 0;
        pl.air_normal_144_y = rs::from_bits(physical.air.landing_normal_144[1]);
        pl.animation_16_x = skater.animation_feedback.published_previous_lateral_tilt[0];
        pl.com_velocity_y = rs::from_bits(physical.reckoning.vector_16[1]);
        pl.offboard_316 = physical.off_board.flag_316 != 0;
        pl.offboard_319 = physical.off_board.flag_319 != 0;
        pl.offboard_time_32 = physical.off_board.scalar_32;
        pl.air_437 = physical.air.known_air_valid_437 != 0;
        pl.air_normal_36 = rs::from_bits(physical.air.landing_normal_32[1]);
        pl.air_remaining_184 = physical.air.scalar_184;
        pl.animation_height_72 = feedback.crouching.animation_height_72;
        motion.prelanding_physical = pl;
    }
    const auto toes = skater.foot_ik.physical_toe_positions(skater.skeleton.record);
    {
        sc::animation::air_leg_extension::Physical a;
        a.com_velocity = f4(physical.reckoning.vector_16);
        a.com_position = f4(physical.reckoning.vector_64);
        a.system_up = f4(physical.reckoning.vector_96);
        a.right_toe = toes[0];
        a.left_toe = toes[1];
        a.animation_height = feedback.crouching.animation_height_72;
        a.offboard_316 = physical.off_board.flag_316 != 0;
        a.remaining_air_time = physical.air.scalar_184;
        motion.air_leg_physical = a;
    }
    {
        graph_host::action::PhysicalConditions c;
        c.requests_dismount = skater.player_state.state_flags[77 - 52];
        c.state = physical.state.state_16;
        skater.animation.action.physical_conditions = c;
    }
    {
        graph_host::motion_gameplay_conditions::GameplayConditions g;
        g.state = physical.state.state_16;
        g.wants_runout = skater.player_state.state_flags[78 - 52];
        g.physics_wiping = skater.player_state.state_flags[59 - 52];
        g.body_flipping = physical.air.flag_441 != 0;
        g.bumped = feedback.bumped;
        g.wants_wipeout = skater.player_state.state_flags[63 - 52] || skater.player_state.state_flags[65 - 52];
        g.grabbing_object = physical.off_board.flag_304 != 0;
        g.retrieving_board = physical.off_board.retrieving_board_323 != 0;
        g.dropping_board = physical.off_board.dropping_board_322 != 0;
        g.in_biped_air = physical.off_board.flag_328 != 0;
        g.hippy_hurdling = physical.off_board.hippy_hurdling_317 != 0;
        g.handplant_flags = physical.air.handplant_flags_324;
        g.handplant_time = physical.air.handplant_time_320;
        g.handplant_thresholds = skater.handplant.animation_thresholds();
        g.footplant_active = physical.air.flag_448 != 0;
        g.footplant_duration = physical.air.footplant_duration_212;
        g.footplant_contact_time = physical.air.footplant_contact_time_208;
        g.time_to_skitch = physical.ground.scalar_276;
        g.skitch_transition_time = profile.skitch_transition_time;
        g.time_to_land = physical.air.scalar_184;
        g.time_to_land_valid = physical.air.known_air_valid_437 != 0;
        g.offboard_trajectory_time = physical.off_board.trajectory_time_120;
        g.offboard_trajectory_valid = physical.off_board.trajectory_valid_331 != 0;
        g.trucks_or_deck_contact = physical.collision.flag_3472 != 0 || physical.collision.flag_3475 != 0;
        g.offboard_time_to_land = physical.off_board.scalar_32;
        g.offboard_air_scalar_92 = physical.off_board.scalar_92;
        g.offboard_air_translation = f4(physical.off_board.vector_96);
        g.offboard_landing_normal = f4(physical.off_board.vector_192);
        g.offboard_committed_to_motion = physical.off_board.flag_329 != 0;
        g.offboard_obstacle_distance = physical.off_board.scalar_112;
        g.offboard_edge_distance = physical.off_board.distance_116;
        // ApexReached82BA5C78 reads Reckoning16.Y, not Air436.
        g.reached_apex = rs::from_bits(physical.reckoning.vector_16[1]) < 0.0f;
        g.can_land_on_board = physical.off_board.flag_316 != 0;
        g.landing_turning = physical.off_board.flag_318 != 0;
        g.grind_contact = physical.grinds.flag_318 != 0 || physical.grinds.flag_322 != 0;
        bool wheel_contact = false;
        for (const auto v : physical.collision.wheel_contact_3296_3299) wheel_contact = wheel_contact || v != 0;
        g.wheel_contact = wheel_contact;
        g.tricks_blocked_on_stairs = false;
        g.moving_object = physical.state.state_16 == 502;
        motion.gameplay_conditions = g;
    }
    skater.animation.action.gameplay_conditions = motion.gameplay_conditions;
    {
        graph_host::motion_native::Physical n;
        n.centre_of_mass_velocity = skater.animated_skeleton.board_frames.com_velocity;
        n.system_up = vec4(physics.riding.reckoning.up);
        // 82DB70A0..70FC publishes Reckoning752, not the physical deck frame.
        n.board_reckoning_z = physics.riding.reckoning_frames.ground[2];
        n.board_reckoning = physics.riding.reckoning_frames.ground;
        motion.native_physical = n;
    }
    motion.bump_acceleration = feedback.ground_acceleration;
    {
        graph_host::motion_native::GesturePhysical g;
        // Original82DB74D8, not the similarly numbered Air output byte.
        g.ground321 = (p.flags_2480 & 0x1000u) != 0;
        g.state_offboard75 = physical.state.category_12 == 500;
        g.selections = profile.gesture_selections;
        g.suppress_up = profile.suppress_up_gesture;
        g.force_brake_bypass = profile.gesture_force_brake_bypass;
        motion.gesture_physical = g;
    }
    // Original ProcessOutput82DB7E38. Probe+72 is copied to Processed1848;
    // Ground240 resets to zero and receives this point only on an interaction.
    const bool interaction_trigger = (p.category_2512 == 100 || p.category_2512 == 500) &&
                                     p.probe_1792.bytes_72_73[0] != 0 && (p.flags_2476 & 0x400000u) != 0;
    std::array<float, 4> direction{};
    if (interaction_trigger) {
        const std::size_t source = (p.flags_2484 & 0x40000u) != 0 ? 0 : 1;
        const auto point = f4(p.probe_1792.vectors_16_32_48[source]);
        const auto inverse = skater.animated_skeleton.roots.world_to_animation;
        for (std::size_t i = 0; i < 4; ++i) {
            direction[i] = rs::mul_add(
                inverse[2][i], point[2],
                rs::mul_add(inverse[1][i], point[1], rs::mul_add(inverse[0][i], point[0], inverse[3][i])));
        }
    }
    {
        graph_host::motion_shove::ShovePhysical s;
        s.interaction_trigger = interaction_trigger;
        s.direction = direction;
        s.in_biped_category = physical.state.category_12 == 500;
        s.board_on_ground = physical.off_board.flag_311 != 0;
        s.animation_height = feedback.crouching.animation_height_72;
        motion.shove_physical = s;
    }
    const auto* filtered = skater.player_state.filtered_output ? &*skater.player_state.filtered_output : nullptr;
    S3G_TRY(auto physical_state,
            grind::publish(motion, physical, filtered, feedback.crouching.animation_height_72, mirrored,
                           physics.riding.motion.effective_basis.columns[2]));
    ConditionInputs conditions;
    conditions.speeds = physics.riding.graph_speeds();
    conditions.physical_state = std::move(physical_state);
    conditions.time_since_last_input = p.time_since_last_input_2748;
    conditions.mirrored = mirrored;
    conditions.riding_fakie = fakie;
    {
        PushBrakeInputs push;
        // Board FillPhysOut82C03304/3318: CollisionInfo+16 ->Ground80.
        push.ground_axis_y = physics.riding.ground.wheel_normal.y;
        push.skeleton_disables_push_brake = skater.foot_ik.state.contacts.support_failed_this_update;
        push.maximum_ground_angle_degrees = profile.maximum_ground_angle_degrees;
        conditions.push_brake = push;
    }
    // Skeleton GetEffectiveRoot82BE3650 negates X/Z iff Processed2476 bit2.
    auto effective_z = skater.animated_skeleton.roots.animation_to_world[2];
    if ((p.flags_2476 & 4u) != 0) {
        for (float& v : effective_z) v = -v;
    }
    auto effective_x = skater.animated_skeleton.roots.animation_to_world[0];
    if ((p.flags_2476 & 4u) != 0) {
        for (float& v : effective_x) v = -v;
    }
    {
        graph_host::motion_riding_conditions::RidingConditionInputs r;
        r.com_velocity = f4(physical.reckoning.vector_16);
        r.skeleton_x = effective_x;
        r.skeleton_z = effective_z;
        r.skate_up_y = deck.basis.columns[1][1];
        r.surface_up_y = physics.riding.ground.wheel_normal.y;
        motion.riding_conditions = r;
    }
    motion.hold_fakie = physics.trainer.hold_fakie;
    AnimationPhysical observations;
    observations.conditions = std::move(conditions);
    observations.feedback = feedback;
    observations.body_tilt.lateral_tilt = physics.riding.reckoning_frames.lateral_tilt[0];
    observations.body_tilt.body_spin_speed = skater.air_reckoning.state.spin_speed;
    observations.body_tilt.filtered_category = physical.filtered_state_0;
    observations.fakie.category = physical.filtered_state_0;
    observations.fakie.grind_state = physical.state.state_16;
    observations.fakie.doing_trick = motion.flags.doing_trick;
    observations.fakie.board_axis = effective_z;
    observations.fakie.deck_velocity = vec4(physics.riding.motion.linear_velocity);
    observations.fakie.external_velocity = skater.centre_of_mass_output.velocity;
    observations.fakie.ground_projected_speed = physics.riding.motion.ground_speed;
    // Original82DB7A58..7AA4 copies AnimOut10374/10371 respectively.
    observations.physical_stance = {skater.animation.packet.regular_stance, skater.animation.packet.riding_switch};
    observations.foot_frame.left_foot = skater.skeleton.record.pose[15][3];
    observations.foot_frame.right_foot = skater.skeleton.record.pose[19][3];
    // 82C02D0C: physical deck body+16, not geometric part translation.
    observations.foot_frame.deck_position = vec4(physics.board.bodies()[deck_index()].rates.position);
    observations.foot_frame.deck_y = axis(deck.basis.columns[1]);
    observations.foot_frame.deck_z = axis(deck.basis.columns[2]);
    // 82DB7598 publishes Motion273 from Processed2468 bit20.
    observations.foot_frame.skateboard_flipped = (p.flags_2468 & (1u << 20)) != 0;
    observations.board_present = physical.off_board.flag_311 != 0;
    observations.physical_28_byte75 = physical.state.category_12 == 500;
    observations.time_since_teleport = motion.riding.time_since_teleport;
    // Fill825999F0/8259C260 publishes discrete off-board inputs before the
    // stock ActionGraph. DerivedControllerInput has already advanced once.
    auto action_intents = controls.action_intents;
    for (const auto& intent : sc::input::grind_intentions::produce(controls.controller)) {
        action_intents.insert(intent.name, intent.value);
    }
    // Fill8259AFFC..B088 emits the four analog AG inputs every frame.
    // Stock AG maps OB_Mag to OB_SteerMagnitude; MG attaches that as ob_Mag
    // and attaches OB_BipedWorldX/Z unchanged. ProcessOutput82DB7288..729C
    // publishes the shared Biped708/400 contact gate/direction as333/288.
    const auto& contact = skater.biped_ground.controller.state.contact;
    {
        sc::input::offboard_intentions::AnalogObservation analog;
        analog.effective_skeleton_z = effective_z;
        analog.biped_correction = contact.active ? std::optional<std::array<float, 4>>(contact.direction) : std::nullopt;
        for (const auto& intent : sc::input::offboard_intentions::produce_analog(controls.controller, analog)) {
            action_intents.insert(intent.name, intent.value);
        }
    }
    for (const auto& intent : sc::input::offboard_intentions::produce_discrete(
             controls.controller, controls.actor_flags, physical.air.use_air_reckoning_452 != 0)) {
        action_intents.insert(intent.name, intent.value);
    }
    AnimationPhaseOutput output = AnimationPhaseOutput::new_();
    S3G_CHECK(skater.animation.advance(graphs, physics.settings.step.simulation.time_step, action_intents,
                                       std::move(observations), output.reset));
    output.publish(skater.animation.packet, profile, controls.actor_flags);
    if (auto reply = skater.teleport_state.take_reply()) {
        output.publish_external_reset(*reply);
    }
    return output;
}

PhysicalFeedback publish_feedback(const GamePhysics& physics, SkaterRuntime& skater) {
    // World8275F430 -> AirCollector82DA7888 publishes the complete mask.
    // This host runs active gameplay with HoM and challenges outside its scope.
    skater.player_input.physical.scoring.capabilities_204 =
        sc::player::conditioner_capabilities::ConditionerCapabilityContext{false, false, false, false}
            .capabilities();
    const auto& p = skater.player_input.processed;
    const auto deck = physics.board.part_transforms()[deck_index()];
    // ProcessOutput82DE53F0 resets the animation packet; CalcLandingQuality
    // 82DE61D0 follows FilteredState82DE5BA0 and uses the completed physical output.
    skater.landing_quality = {};
    if (skater.player_state.filtered_output.has_value()) {
        const auto& filtered = *skater.player_state.filtered_output;
        sc::animation::landing_quality::Input input;
        input.previous_filtered_state = static_cast<std::uint32_t>(filtered.previous_category);
        input.filtered_state = static_cast<std::uint32_t>(filtered.category);
        input.ground_normal = vec4(physics.riding.ground.overall_normal);
        input.deck_velocity = vec4(physics.riding.motion.linear_velocity);
        input.flipped = (p.flags_2468 & (1u << 20)) != 0;
        // Board Fill82C02AD8..2B1C publishes physical part6 Z to
        // bundle0+32; this is separate from Reckoning752's ground frame.
        input.reckoning_forward = axis(deck.basis.columns[2]);
        // Ground Enter clears this owner before publication, as in82D375C8.
        input.air_spin = skater.air_reckoning.state.spin_speed;
        skater.landing_quality.update(input, skater.landing_quality_settings);
    }
    ReckoningFeedback reckoning{
        // Skeleton16144 -> PhysOutSystemReckoning64,82BE1D84/8C.
        vec3(skater.animated_skeleton.board_frames.centre_of_mass),
        physics.riding.reckoning.up,
        deck.translation,
        physics.riding.reckoning_frames.target_lean_angle,
    };
    ControlFeedback control{p.flags_2468, skater.animation_input.fields.turn, skater.animation.packet.mirrored};
    sc::animation::ground_acceleration::Input acceleration;
    acceleration.deck = solve::deck_frame(physics.board);
    acceleration.ground = physics.riding.reckoning_frames.ground;
    acceleration.world_acceleration = vec4(physics.riding.ground.accelerations[deck_index()]);
    const PhysicalFeedback feedback =
        skater.animation_feedback.update(physics.riding.motion, skater.ground.pumping, skater.ground.wobble, reckoning,
                                         control, acceleration, physics.riding.reckoning_frames.lateral_tilt);
    skater.physical_feedback = feedback;
    return feedback;
}

PhysicalFeedback initial_feedback() {
    PhysicalFeedback out{};
    out.turning.field_32 = 0.0f;
    out.turning.field_36 = 0.0f;
    out.turning.field_52 = 0.0f;
    out.turning.field_56 = 0.0f;
    out.turning.field_60 = 0.0f;
    out.turning.body_168 = 0.0f;
    out.crouching.body_84 = 0.0f;
    out.crouching.body_164 = 0.0f;
    out.crouching.body_188 = 0.0f;
    out.crouching.force_516 = 0.0f;
    out.crouching.ground_force_520 = 0.0f;
    out.crouching.minimum_crouch_528 = 0.0f;
    out.crouching.deck_angle_532 = 0.0f;
    out.crouching.animation_height_72 = 0.0f;
    out.pumping_acceleration = 0.0f;
    out.ground_acceleration = {0.0f, 0.0f, 0.0f, 0.0f};
    // Animation reset vector112 is zero; stock bumps threshold is positive100.
    out.bumped = false;
    out.conditioned_turn = {};
    return out;
}

sc::math::Vector3 vec3(std::array<float, 4> v) { return Vector3::new_(v[0], v[1], v[2]); }

std::array<float, 4> vec4(sc::math::Vector3 v) { return {v.x, v.y, v.z, 0.0f}; }

std::array<float, 4> axis(std::array<float, 3> v) { return {v[0], v[1], v[2], 0.0f}; }

}  // namespace s3::game::physics::animation_phase
