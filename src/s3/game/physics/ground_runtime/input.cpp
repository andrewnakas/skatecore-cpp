// Port of skate-game/src/physics/ground_runtime/input.rs.
//! UpdateSkateboard82D38800 inputs from the actual processed packet, animation
//! attributes, physical pose and source board toolkit. No raw button-to-force
//! or guessed contact flags are introduced at this boundary.
#include "s3/game/physics/ground_runtime/input.hpp"

#include "s3/core/riding/ground_correction_math.hpp"
#include "s3/game/physics/animated_skeleton.hpp"
#include "s3/game/physics/animation_input.hpp"
#include "s3/game/physics/ground_runtime.hpp"
#include "s3/game/physics/riding_outputs.hpp"

namespace s3::game::physics::ground_runtime {

using input::raw;

sc::physics::board_toolkit::BoardToolkit GroundRuntime::prepare_toolkit(
    const sc::physics::board_runtime::BoardRuntime& board, const sc::player::input_phase::ProcessedPhysicsInput& p) {
    const auto result = sc::physics::board_toolkit::BoardToolkit::from_board(
        board, p.flags_2468, p.scalar_2612, raw(p.vectors_464_480_496_512_528[0]), retained_board_normal);
    retained_board_normal = result.filtered_normal;
    return result;
}

}  // namespace s3::game::physics::ground_runtime

namespace s3::game::physics::ground_runtime::settings {

using input::raw;
using input::v;
using input::xyz;
using sc::riding::ground_correction_math::dot_product;

GroundBoardInput GroundSettings::input(const sc::physics::board_toolkit::BoardToolkit& t,
                                       const sc::player::input_phase::ProcessedPhysicsInput& p,
                                       const animation_input::AnimationInput& a,
                                       const sc::riding::pumping::state::PumpingState& pumping,
                                       float unintentional_pump_scalar, const riding_outputs::RidingOutputs& riding,
                                       const animated_skeleton::AnimatedSkeleton& skeleton,
                                       std::array<sc::physics::drive_frames::RetailAffineTransform, 2> base_trucks,
                                       input::GroundInputObservations extra) const {
    const auto& f = a.fields;
    const auto normal = raw(p.vectors_464_480_496_512_528[0]);
    const auto velocity = raw(p.vectors_400_416[0]);
    const auto angular = raw(p.vectors_400_416[1]);
    const auto axis544 = raw(p.vectors_544_560_592_608[0]);
    const auto ground_normal = v(riding.reckoning.ground_normal);
    const auto ground = riding.reckoning_frames.ground;
    std::array<float, 4> edge_delta{};
    for (std::size_t i = 0; i < 4; ++i) edge_delta[i] = t.deck[3][i] - extra.edge_point[i];
    const auto hang_axis = raw(p.vectors_720_784_800_816_832_864[4]);
    GroundBoardInput in;
    in.steering.turn = f.turn;
    in.steering.hard_turn = f.hard_turn;
    in.steering.absolute_body_speed = t.absolute_speed;
    in.steering.flipped_controls_scalar = t.control_sign;
    in.steering.balance = f.balance;
    in.steering.truck_tightness = p.truck_tightness_2760;
    in.steering.pushing = (p.flags_2468 & 0x08000000u) != 0;
    //Ground's core controller replaces tilt and height before consuming
    //them; these are deliberately not used as substitute observations.
    in.speed_wobble.tilt = 0.0f;
    in.speed_wobble.speed = p.scalar_2612;
    in.speed_wobble.center_of_mass_height = 0.0f;
    in.speed_wobble.truck_tightness = p.truck_tightness_2760;
    in.speed_wobble.activation_threshold = wobble_activation;
    in.speed_wobble.amplitude_multiplier = wobble_amplitude;
    in.truck_flags_2468 = p.flags_2468;
    in.truck_flags_2472 = p.flags_2472;
    in.contact = riding.probes.wall_contact_frame(p.time_on_ground_2752, p.signed_ground_time_2756);
    in.ground_vector_1216 = ground_normal;
    in.propulsion.flags_2468 = p.flags_2468;
    in.propulsion.flags_2472 = p.flags_2472;
    in.propulsion.target_speed = a.contacts.push_speed * push_target_multiplier;
    in.propulsion.signed_speed = p.scalar_2612;
    in.propulsion.absolute_body_speed = t.absolute_speed;
    in.propulsion.scalar_2660 = t.total_mass;
    in.propulsion.timestep = p.timestep_2604;
    in.propulsion.brake_input = f.brake;
    in.propulsion.push_direction = xyz(t.forward);
    in.propulsion.brake_direction = xyz(t.horizontal_forward);
    in.propulsion.surface_braking_factor = surface_braking_factor;
    in.ground_force.argument_1_2752 = p.time_on_ground_2752;
    in.ground_force.processed_2776 = p.crouch_2776;
    in.ground_force.processed_2780 = p.crouch_delta_2780;
    in.ground_force.previous_state_scalar_56 = pumping.absorption;
    in.ground_force.ground_scalar_1216 = foot_force_offset;
    in.ground_force.ground_scalar_1232 = absorption_front;
    in.ground_force.ground_scalar_1236 = absorption_rear;
    in.ground_force.ground_scalar_1240 = foot_force_offset;
    in.ground_force.balance_2720 = f.balance;
    in.ground_force.surface_speed_2656 = p.scalar_2656;
    in.ground_force.axis_384 = t.transverse_up;
    in.ground_force.velocity_400 = velocity;
    in.ground_force.axis_544 = axis544;
    in.speed_model.flags_2468 = p.flags_2468;
    in.speed_model.flags_2476 = p.flags_2476;
    in.speed_model.wheel_contact_count = static_cast<std::int32_t>(p.wheel_count_2556);
    in.speed_model.frames_2580 = static_cast<std::int32_t>(p.spin_same_direction_frames_2580);
    in.speed_model.timestep = p.timestep_2604;
    in.speed_model.signed_speed = p.scalar_2612;
    in.speed_model.angle_2652 = p.scalar_2652;
    in.speed_model.surface_speed = p.scalar_2656;
    in.speed_model.turn_2672 = p.spin_input_2672;
    in.speed_model.balance = f.balance;
    in.speed_model.elapsed_without_input = p.time_since_last_input_2748;
    in.speed_model.manual_state_276 = 0.0f;
    in.speed_model.vector_160 = t.effective[2];
    in.speed_model.vector_352 = t.forward;
    in.speed_model.velocity_416 = angular;
    in.speed_model.normal_464 = normal;
    in.speed_model.effective_forward = t.effective[2];
    in.speed_model.mass = t.total_mass;
    in.slide_friction.heading_time = 0.0f;
    in.slide_friction.normal = normal;
    in.slide_friction.velocity = velocity;
    in.slide_friction.side_axis = t.deck[0];
    in.slide_friction.surface_speed = p.scalar_2656;
    in.slide_friction.scalar_2764 = p.scalar_2764;
    in.pump_force.flags_2476 = p.flags_2476;
    in.pump_force.mode_multiplier = unintentional_pump_scalar;
    in.pump_force.pumping_scalar = pumping.pump_acceleration;
    in.pump_force.input_scalar_2660 = t.total_mass;
    in.pump_force.timestep = p.timestep_2604;
    in.pump_force.direction_432 = t.forward_velocity;
    {
        const float th = rs::from_bits(0x358637bdu);
        in.pump_force.normal_threshold = {th, th, th, th};
    }
    in.straighten.heading_time = 0.0f;
    in.straighten.scalar_2764 = p.scalar_2764;
    in.straighten.turn = f.turn;
    in.straighten.forward = t.forward;
    in.straighten.velocity = velocity;
    in.straighten.normal = normal;
    in.heading.balance = f.balance;
    in.heading.manual_turn = p.spin_input_2672;
    in.heading.flags_2472 = p.flags_2472;
    in.heading.timestep = p.timestep_2604;
    in.heading.signed_speed = p.scalar_2612;
    in.heading.manual_curve_input = p.scalar_2652;
    in.heading.turn_2712 = f.raw_turn;
    in.heading.scalar_2740 = riding.heading_adjust_factor();
    in.heading.velocity = t.forward_velocity;
    in.heading.normal = normal;
    in.heading.angular_velocity = raw(p.vectors_720_784_800_816_832_864[0]);
    in.heading.transform = ground;
    in.anti_flip.flags_2468 = p.flags_2468;
    in.anti_flip.balance = f.balance;
    in.anti_flip.axis_96 = t.deck[2];
    in.anti_flip.axis_64 = t.deck[0];
    in.anti_flip.projection_axis_544 = axis544;
    in.manual.balance = f.balance;
    in.manual.flipped_controls = t.control_sign;
    in.manual.procedural_noise_time = p.state_timer_2664;
    in.manual.absolute_speed = t.absolute_speed;
    in.manual.animation_noise = skeleton.board_at_y_delta;
    in.manual.timestep = p.timestep_2604;
    in.manual.powersliding = false;
    in.manual.braking = (p.flags_2468 & 0x20000000u) != 0;
    in.manual.positive_balance_contact = (p.flags_2472 & 0x08000000u) != 0;
    in.manual.negative_balance_contact = (p.flags_2472 & 0x04000000u) != 0;
    in.manual.reversed_point_selection = (p.flags_2468 & 0x00100000u) != 0;
    in.manual.reference_x = ground[0];
    in.manual.reference_z = ground[2];
    in.manual.deck_z = t.deck[2];
    in.manual.velocity_frame_z = raw(p.effective_anim_transform_192[2]);
    in.manual.angular_velocity_world = angular;
    in.manual.correction_point_7888 = v(base_trucks[0].translation);
    in.manual.correction_point_7952 = v(base_trucks[1].translation);
    in.ground_drag.flags_2468 = p.flags_2468;
    in.ground_drag.absolute_body_speed_2616 = t.absolute_speed;
    in.ground_drag.balance_2720 = f.balance;
    in.ground_drag.scalar_2724 = extra.manual_drag_2724;
    in.ground_drag.ground_normal_y = ground_normal[1];
    in.contact_time_2756 = p.signed_ground_time_2756;
    in.trajectory_state_1776_bits = extra.trajectory_state_bits;
    in.anti_flip_nudge.deck_speed_2652 = p.scalar_2652;
    in.anti_flip_nudge.deck_axis_96 = t.deck[2];
    in.hang_up.flags_1516 = extra.edge_flags;
    in.hang_up.deck_speed_2652 = p.scalar_2652;
    in.hang_up.scalar_84 = t.deck[1][1];
    in.hang_up.geometry_axis_dot_positive = dot_product(hang_axis, hang_axis) > 0.0f;
    in.halfpipe_wheel_catch.flags_1516 = extra.edge_flags;
    in.halfpipe_wheel_catch.deck_speed_2652 = p.scalar_2652;
    in.halfpipe_wheel_catch.deck_x_axis_y = t.deck[0][1];
    in.halfpipe_wheel_catch.deck_y_axis_y = t.deck[1][1];
    in.halfpipe_wheel_catch.signed_deck_distance = dot_product(t.deck[1], edge_delta);
    in.pinning.flags_2488 = p.flags_2488;
    in.pinning.frames_since_teleport_2584 = static_cast<std::int32_t>(p.frames_since_teleport_2584);
    in.pinning.flags_2472 = p.flags_2472;
    return in;
}

}  // namespace s3::game::physics::ground_runtime::settings

namespace s3::game::physics::ground_runtime::input {

std::array<float, 4> raw(std::array<std::uint32_t, 4> v) { return rs::from_bits(v); }

sc::math::Vector3 xyz(std::array<float, 4> v) { return sc::math::Vector3::new_(v[0], v[1], v[2]); }

std::array<float, 4> v(sc::math::Vector3 v) { return {v.x, v.y, v.z, 0.0f}; }

}  // namespace s3::game::physics::ground_runtime::input
