// Port of skate-core/src/player/offboard/controller.rs and controller/{output,
// placement,state,step}.rs.
#include "s3/core/player/offboard/controller.hpp"

#include "s3/core/player/offboard/ground_branch.hpp"

namespace s3::player::offboard::controller {

// ------------------------------------------------------------------ output.rs
// (build_frame lives in controller/output.cpp)
GroundResult export_(const State& s) {
    GroundResult r;
    r.physical_frame = s.motion.published_frame_64;
    r.animation_frame = s.frame_output.frame;
    r.surface_frame = build_frame(s.surface.spring_normal, s.motion.frame_0[2]);
    r.velocity = s.frame_output.velocity;
    r.position = s.position_368;
    r.angular_velocity = s.motion.angular_velocity_688;
    r.alternate = s.alternate_709;
    r.sliding = s.sliding.active_710;
    return r;
}

// ------------------------------------------------------------------ state.rs
namespace {
ground_motion::GroundMotionState reset_motion(Vector correction_576) {
    ground_motion::GroundMotionState m;
    m.frame_0 = IDENTITY;
    m.published_frame_64 = IDENTITY;
    m.previous_support_frame_192 = IDENTITY;
    m.support_velocity_256 = ZERO;
    m.predicted_support_velocity_272 = ZERO;
    m.previous_support_velocity_288 = ZERO;
    m.support_acceleration_304 = ZERO;
    m.filtered_local_acceleration_320 = ZERO;
    m.support_yaw_336 = 0.0f;
    m.previous_support_yaw_340 = 0.0f;
    m.predicted_support_yaw_344 = 0.0f;
    m.support_speed_348 = 0.0f;
    m.support_id_352 = 0;
    m.velocity_480 = ZERO;
    m.correction_576 = correction_576;
    m.target_frame_608 = IDENTITY;
    m.angular_velocity_688 = 0.0f;
    m.speed_704 = 0.0f;
    m.correction_enabled_711 = false;
    m.support_velocity_removed_715 = false;
    m.target_scale_784 = -1.0f;
    return m;
}
contact_correction::ContactCorrection fresh_contact() {
    contact_correction::ContactCorrection c;
    c.active = false;
    c.direction = ZERO;
    c.displacement = ZERO;
    return c;
}
slide::SpecialMode fresh_special() {
    slide::SpecialMode s;
    s.enabled_714 = false;
    s.elapsed_788 = 0.0f;
    return s;
}
slide::Sliding fresh_sliding() {
    slide::Sliding s;
    s.velocity_528 = ZERO;
    s.active_710 = false;
    return s;
}
position_output::FrameOutput fresh_frame_output() {
    position_output::FrameOutput f;
    f.frame = IDENTITY;
    f.velocity = ZERO;
    return f;
}
}  // namespace

State State::new_(const std::array<std::optional<ClipMetric>, 3>& metrics) {
    const auto speeds = rs::map(metrics, [](const std::optional<ClipMetric>& m) { return m ? m->speed() : 0.0f; });
    State s;
    s.motion = reset_motion(ZERO);
    s.intent = movement_intent::State{};
    s.surface = surface_frame::State{};
    s.contact = fresh_contact();
    s.special = fresh_special();
    s.sliding = fresh_sliding();
    s.frame_output = fresh_frame_output();
    s.cadence = cadence::BipedCadence{};
    s.thresholds = cadence::CadenceThresholds::from_clip_speeds(speeds[0], speeds[1], speeds[2]);
    s.position_368 = ZERO;
    s.correction_target_592 = ZERO;
    s.forward_delta_692 = 0.0f;
    s.right_delta_696 = 0.0f;
    s.velocity_override_remaining_772 = -1.0f;
    s.alternate_709 = false;
    return s;
}

void State::reset() {
    motion = reset_motion(motion.correction_576);
    intent = movement_intent::State{};
    surface = surface_frame::State{};
    contact = fresh_contact();
    special = fresh_special();
    sliding = fresh_sliding();
    frame_output = fresh_frame_output();
    position_368 = ZERO;
    forward_delta_692 = 0.0f;
    right_delta_696 = 0.0f;
    velocity_override_remaining_772 = -1.0f;
    alternate_709 = false;
}

// ------------------------------------------------------------------ placement.rs
namespace {
namespace place_math {
float dot(Vector a, Vector b) { return (a[0] * b[0] + a[1] * b[1]) + a[2] * b[2]; }
Vector sub(Vector a, Vector b) { return Vector{a[0] - b[0], a[1] - b[1], a[2] - b[2], a[3] - b[3]}; }
float length(Vector v) {
    const float q = dot(v, v);
    float r = 1.0f / rs::sqrt(q);
    for (int i = 0; i < 2; ++i) {
        r = rs::mul_add(r * 0.5f, rs::mul_add(-q, r * r, 1.0f), r);
    }
    const float n = q * r;
    return q == 0.0f ? 0.0f : n;
}
}  // namespace place_math
}  // namespace

void State::place(const PlacementInput& i) {
    using namespace place_math;
    if (i.current_state == 500) {
        reset();
        const Vector delta = sub(correction_target_592, i.previous_frame[3]);
        const bool eligible = i.previous_state == 501 && !(dot(i.frame[2], delta) <= 0.0f) &&
                              !(dot(i.frame[2], i.previous_frame[2]) <= rs::from_bits(0x3f666666u));
        if (eligible) {
            const Vector difference = sub(i.previous_frame[3], correction_target_592);
            const float amount = dot(i.previous_frame[2], difference);
            motion.correction_576 = rs::map(i.previous_frame[2], [&](float v) { return v * amount; });
            motion.correction_enabled_711 = true;
        } else {
            clear_entry_correction();
        }
    } else if (i.current_state == 501) {
        clear_entry_correction();
    }
    motion.frame_0 = i.frame;
    motion.published_frame_64 = i.frame;
    frame_output.frame = i.frame;
    position_368 = i.body_position;
    motion.velocity_480 = i.velocity;
    motion.speed_704 = length(i.velocity);
    surface.spring_normal = i.frame[1];
    surface.spring_delta = ZERO;
}

void State::clear_entry_correction() {
    motion.correction_576 = ZERO;
    correction_target_592 = ZERO;
    motion.correction_enabled_711 = false;
}

// ------------------------------------------------------------------ controller.rs
GroundResult step_ground(State& state, const Settings& settings, const GroundJob& job) {
    step::update(state, settings, job);
    return export_(state);
}

// ------------------------------------------------------------------ step.rs
namespace step {
namespace {
std::array<float, 3> xyz(Vector v) { return {v[0], v[1], v[2]}; }

void velocity(State& s, const Settings& c, const GroundJob& j) {
    movement_velocity::State state;
    state.velocity = s.motion.velocity_480;
    state.turn = s.motion.angular_velocity_688;
    state.forward_delta = s.forward_delta_692;
    state.right_delta = s.right_delta_696;
    state.speed = s.motion.speed_704;
    state.override_remaining = s.velocity_override_remaining_772;
    movement_velocity::Input in;
    in.forward = s.motion.frame_0[2];
    in.right = s.motion.frame_0[0];
    in.plane_normal = s.surface.surface_normal;
    in.desired_speed = s.intent.speed;
    in.steering = s.intent.steering;
    in.slope_mode = s.special.enabled_714;
    in.obstacle = s.contact.active;
    in.obstacle_normal = s.contact.direction;
    in.override_gate = j.requested_phase;
    in.override_duration = j.override_duration;
    in.override_velocity = j.animation_velocity;
    in.flags = j.flags;
    state.update(c.movement_velocity, in);
    s.motion.velocity_480 = state.velocity;
    s.motion.angular_velocity_688 = state.turn;
    s.motion.speed_704 = state.speed;
    s.forward_delta_692 = state.forward_delta;
    s.right_delta_696 = state.right_delta;
    s.velocity_override_remaining_772 = state.override_remaining;
}
}  // namespace

void update(State& s, const Settings& c, const GroundJob& j) {
    s.intent.speed = 0.0f;
    s.intent.steering = 0.0f;
    s.special.update(j.movement, j.flags, s.motion.frame_0[2][1]);
    {
        contact_correction::Input in;
        in.collision_displacements = j.collision_displacements;
        in.projection_axis_416 = s.surface.spring_normal;
        in.up_axis_16 = s.motion.frame_0[1];
        s.contact.update(in);
    }
    {
        slide::Input in;
        in.special_mode_714 = s.special.enabled_714;
        in.surface_normal_560 = s.surface.source_normal;
        in.movement_velocity_480 = s.motion.velocity_480;
        if (s.contact.active) in.contact_direction_400 = s.contact.direction;
        s.sliding.update(in, c.slide_vs_slope, c.slide_vs_speed);
    }
    {
        movement_intent::Input in;
        in.flags = j.flags;
        in.suppress_minimum = j.suppress_minimum;
        in.magnitude = j.movement;
        in.steering = j.steering;
        in.sprint_pressed = j.sprint_pressed;
        in.edge_active = j.edge_active;
        in.ignore_obstacle = j.ignore_obstacle;
        in.direction = j.desired_direction;
        in.edge_tangent = j.target_frame[2];
        in.edge_point = j.target_frame[3];
        in.right = s.motion.frame_0[0];
        in.up = s.motion.frame_0[1];
        in.forward = s.motion.frame_0[2];
        in.position = s.motion.frame_0[3];
        in.obstacle = s.contact.active;
        in.obstacle_normal = s.contact.direction;
        in.sliding = s.sliding.active_710;
        in.slide_velocity = s.sliding.velocity_528;
        s.intent.update(c.movement_intent, in);
    }
    {
        ground_branch::BranchInput in;
        in.movement_velocity_480 = s.motion.velocity_480;
        in.output_velocity_512 = s.frame_output.velocity;
        in.right_0 = s.motion.frame_0[0];
        in.position_48 = s.motion.frame_0[3];
        in.contact_point_96 = j.target_position;
        in.contact_flags_176 = j.flags;
        in.suppressed_317 = j.suppress_lean;
        s.alternate_709 = ground_branch::select_alternate(in);
    }
    if (s.alternate_709) {
        const Vector forward = s.motion.frame_0[2];
        ground_branch::integrate_alternate(s.motion.velocity_480, s.motion.speed_704, s.motion.frame_0[3],
                                           s.frame_output, forward, s.surface.final_up, s.surface.spring_normal);
        return;
    }
    velocity(s, c, j);
    {
        surface_frame::SurfaceInput in;
        in.flags = j.flags;
        in.normal = j.contact_normal;
        in.origin = j.contact_position;
        in.edge_point = j.edge_position;
        in.edge_normal = j.edge_normal;
        s.surface.update_surface(in);
    }
    {
        surface_frame::SpringInput in;
        in.velocity = s.motion.velocity_480;
        in.up = s.motion.frame_0[1];
        in.right = s.motion.frame_0[0];
        in.forward = s.motion.frame_0[2];
        in.spring_right = s.frame_output.frame[0];
        in.spring_forward = s.frame_output.frame[2];
        in.right_delta = s.right_delta_696;
        in.forward_delta = s.forward_delta_692;
        in.suppress_lean = j.suppress_lean;
        s.surface.update_spring(in);
    }
    {
        ground_motion::GroundMotionInput in;
        in.contact_position_0 = j.contact_position;
        in.contact_frame_32 = j.support_frame;
        in.contact_target_96 = j.target_position;
        in.contact_normal_112 = j.target_normal;
        in.contact_flags_176 = j.flags;
        in.contact_id_180 = j.support_id;
        in.animation_motion_224 = j.animation_motion;
        in.animation_motion_240 = j.animation_velocity;
        in.requested_duration_288 = j.requested_duration;
        in.mirrored_292 = j.mirrored;
        in.animation_directed_304 = j.animation_directed;
        in.target_frame_present_352 = j.target_frame_present;
        in.target_frame_368 = j.target_frame;
        in.reference_frame_128 = s.frame_output.frame;
        in.contact_displacement_384 = s.contact.displacement;
        in.velocity_addition_528 = s.sliding.velocity_528;
        in.desired_up_544 = s.surface.surface_normal;
        in.correction_target_592 = s.correction_target_592;
        in.obstacle_target_672 = s.intent.edge_target;
        in.obstacle_enabled_713 = s.intent.edge_aligned;
        ground_motion::update(s.motion, in);
    }
    s.frame_output.update(s.motion.frame_0[2], s.surface.final_up, s.surface.spring_normal, s.motion.frame_0[3]);
    {
        position_output::PositionInput in;
        in.previous_origin_112 = s.motion.published_frame_64[3];
        in.override_origin_592 = s.correction_target_592;
        in.use_override_711 = s.motion.correction_enabled_711;
        in.projection_axis_416 = s.surface.spring_normal;
        in.contact_flags_176 = j.flags;
        in.contact_origin_0 = j.contact_position;
        in.contact_origin_96 = j.target_position;
        in.frame_position_176 = s.frame_output.frame[3];
        in.animation_position_272 = j.animation_position;
        position_output::update_position(s.position_368, in);
    }
    {
        cadence::CadenceInput in;
        in.motion_512 = xyz(s.frame_output.velocity);
        in.motion_reference_272 = xyz(s.motion.predicted_support_velocity_272);
        in.reject_axis_400 = xyz(s.contact.direction);
        in.reject_enabled_708 = s.contact.active;
        in.up_144 = xyz(s.frame_output.frame[1]);
        in.frame_rows_0_16_32 = {xyz(s.motion.frame_0[0]), xyz(s.motion.frame_0[1]), xyz(s.motion.frame_0[2])};
        in.frame_position_48 = xyz(s.motion.frame_0[3]);
        in.animation_motion_224 = xyz(j.animation_motion);
        in.requested_duration_288 = j.requested_duration;
        in.requested_phase_296 = j.requested_phase;
        in.suppress_adjustment_353 = j.edge_active;
        in.contact_flags_176 = j.flags;
        in.contact_point_96 = xyz(j.target_position);
        s.cadence.update(in, s.thresholds);
    }
}

}  // namespace step

}  // namespace s3::player::offboard::controller
