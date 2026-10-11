// Port of skate-game/src/physics/known_air/runtime.rs.
//! Concrete KnownAir boundary. All callbacks mutate the same live owners.
//! Fallible host queries/pose checks retain their first error for the phase caller.
#include "s3/game/physics/known_air/runtime.hpp"

#include "s3/game/physics.hpp"
#include "s3/game/physics/footplant.hpp"
#include "s3/game/physics/input_phase.hpp"
#include "s3/game/physics/skater.hpp"
#include "s3/game/physics/skeleton_input_runtime.hpp"
#include "s3/game/physics/solve.hpp"
#include "s3/game/physics/wipeout.hpp"
#include "s3/game/physics/wipeout/observations.hpp"

namespace s3::game::physics::known_air::runtime {

using namespace ::s3::game::physics::known_air::math;
using ::s3::game::physics::footplant::FootplantFrame;
using ::s3::game::physics::skeleton_input_runtime::SkeletonOwners;

rs::Result<Runtime, std::string> Runtime::new_(GamePhysics& physics, SkaterRuntime& skater) {
    const auto& selector = skater.trajectory.selector;
    const auto* selection = selector.selection();
    if (selection == nullptr) return rs::make_err(std::string("KnownAir requires its actual winning query result"));
    const auto local_com = selector.local_com_position();
    if (!local_com) return rs::make_err(std::string("KnownAir requires selector local COM"));
    const auto board_position = selector.board_position();
    if (!board_position) return rs::make_err(std::string("KnownAir requires selector board position"));
    if (!skater.player_input.toolkit) return rs::make_err(std::string("KnownAir requires prepared board toolkit"));
    return Runtime(physics, skater, *skater.player_input.toolkit, *selection, *local_com, *board_position);
}

Status Runtime::finish() {
    if (error) return rs::make_err(*error);
    return ok();
}

void Runtime::record(const Status& result) {
    if (!error) {
        error = result.err();
    }
}

KnownAirTrajectory trajectory(Trajectory t) {
    //82E09A14/2C splats the request horizon into all four final-vector lanes.
    //KnownAir Fill copies all64 bytes, including the three repeated words.
    KnownAirTrajectory k;
    k.position = t.position;
    k.velocity = t.velocity;
    k.acceleration = t.acceleration;
    k.scalar_48 = t.duration;
    k.word_52 = rs::to_bits(t.duration);
    k.word_56 = rs::to_bits(t.duration);
    k.word_60 = rs::to_bits(t.duration);
    return k;
}

Trajectory actual(const KnownAirTrajectory& t) {
    Trajectory a;
    a.position = t.position;
    a.velocity = t.velocity;
    a.acceleration = t.acceleration;
    a.duration = t.scalar_48;
    return a;
}

void Runtime::enable_board_angular_drive_only() {
    physics.board.hook_mut().drive.enable_angular_only(skater.ground_lifecycle.board_animated_290);
}

void Runtime::set_air_collision_update_enabled(bool v) { skater.ground_lifecycle.skeleton_controller.flag_18 = v; }

void Runtime::set_skeleton_collision_state(std::uint32_t state) {
    const auto result = skater.ground_lifecycle.skeleton_controller.request(state, skater.skeleton_collision);
    record(result);
}

void Runtime::set_skeleton_physics_truck_tilt_enabled(bool v) { skater.ground_lifecycle.skeleton_elapsed_16505 = v; }

void Runtime::set_skeleton_inverse_kinematics_enabled(bool v) { skater.foot_ik.enable_feet(v); }

void Runtime::reset_reckoning_flipping() { reset_flip(); }

void Runtime::set_footplant_flag_240(bool v) { skater.footplant.enabled = v; }

void Runtime::reset_footplants() { skater.footplant.reset(); }

float Runtime::board_transform_height() { return physics.board.part_transforms()[6].translation.y; }

V Runtime::skeleton_com_position() { return skater.animated_skeleton.board_frames.centre_of_mass; }

bool Runtime::selector_targeting_grind() const { return skater.trajectory.selector.grind_locked_to_middle(); }

void Runtime::start_grind_air_adjust_from_selector() {
    if (selection.grind) {
        const auto target = *selection.grind;
        skater.skeleton_input.grind_air.start(target.air_target());
        skater.skeleton_input.grind_air_started = true;
    } else {
        record(rs::make_err(std::string("KnownAir grind lock has no accepted trajectory target")));
    }
}

std::pair<std::int32_t, V> Runtime::selector_closest_trajectory_point(V com, V zero) {
    const auto t = selection.com_trajectory;
    float best = 100000000.0f;
    V offset = zero;
    for (std::int32_t i = 0; i < 100; ++i) {
        const V d = sub(com, t.position_at(float(i) * rs::from_bits(0x3c888889u)));
        const float squared = dot(d, d);
        if (squared > best) {
            return {i - 1, offset};
        }
        best = squared;
        offset = d;
    }
    return {0, zero};
}

KnownAirPrediction Runtime::selected_prediction() const {
    KnownAirPrediction p;
    p.trajectory = trajectory(selection.prediction.request.trajectory);
    p.collision_time_48 = selection.prediction.result.contact_time;
    p.collision_frame_128 = selection.prediction.result.contact_frame;
    return p;
}

KnownAirTrajectory Runtime::selector_trajectory_2704() const { return trajectory(selection.com_trajectory); }

V Runtime::selector_contact_position() { return selection.prediction.result.contact_position; }

V Runtime::selector_vector_2848() const { return board_position; }

V Runtime::selector_com_position_2784() const { return local_com; }

std::pair<V, float> Runtime::highest_trajectory_position(const KnownAirTrajectory& t) {
    return actual(t).highest_position();
}

bool Runtime::selector_has_just_changed() const { return skater.trajectory.selector.just_changed(); }

void Runtime::reset_trajectory_selector() { skater.trajectory.selector.reset(); }

void Runtime::begin_reckoning_body_flip(bool side) { begin_flip(side); }

void Runtime::update_air_collision() {
    const auto& p = skater.player_input.processed;
    const auto result = skater.ground_lifecycle.skeleton_controller.update_air(
        p.flags_2472, rs::from_bits(p.vectors_400_416[0][1]), skater.skeleton_collision);
    record(result);
}

bool Runtime::grind_air_adjust_started() const { return skater.skeleton_input.grind_air_started; }

void Runtime::set_grind_air_adjust_started(bool v) { skater.skeleton_input.grind_air_started = v; }

void Runtime::set_grind_air_adjust_activated(bool v) { skater.skeleton_input.grind_air_active = v; }

bool Runtime::grind_air_adjust_adjusting() const { return skater.skeleton_input.grind_air_adjusting; }

void Runtime::disable_grind_foot_collision(GrindFootBody body) {
    std::size_t part = 0;
    switch (body) {
        case GrindFootBody::Body3152: part = 15; break;
        case GrindFootBody::Body3156: part = 16; break;
        case GrindFootBody::Body3160: part = 17; break;
        case GrindFootBody::Body3168: part = 19; break;
        case GrindFootBody::Body3172: part = 20; break;
        case GrindFootBody::Body3176: part = 21; break;
    }
    auto& collision = skater.skeleton_collision;
    collision.pending_reenable = true;
    collision.disable_count[part] = 2;
    collision.parts[part].enabled = false;
}

void Runtime::update_reckoning_air_states(V n, float blend, float spin, float flip) {
    const auto r = skater.air_reckoning.update(physics.riding, skater.player_input.processed,
                                               skater.animation_input.extra.physical_body_spin, n, blend, spin, flip);
    if (r.is_err()) {
        record(rs::make_err(r.unwrap_err()));
    } else {
        record(ok());
    }
}

void Runtime::update_known_air_skeleton(V target) {
    const auto collision = input_phase::collision(skater);
    auto& s = skater;
    SkeletonOwners owners{
        s.animated_skeleton, s.skeleton,        s.skeleton_drives, s.foot_ik, s.animation_input,
        s.skeleton_output.correction, s.pose_errors,
    };
    const auto& packet = s.animation.packet;
    const auto r = s.skeleton_input.update_known_air(s.skeleton_air, physics.board,
                                                     physics.riding.reckoning_frames.system, target, packet,
                                                     s.player_input.processed, owners, packet.hierarchy, collision,
                                                     physics.settings.step.simulation);
    if (r.is_err()) {
        record(rs::make_err(r.unwrap_err()));
    } else {
        record(ok());
    }
}

void Runtime::set_skeleton_add_skateboard_error(bool v) { skater.skeleton_output.correction.pending = v; }

void Runtime::set_head_tracking_target(V target, bool valid) {
    skater.skeleton_input.head_tracking_history[5] = target;
    skater.skeleton_input.head_tracking_active = valid;
}

void Runtime::update_board_steering_tilt(float tilt) {
    const auto& p = skater.player_input.processed;
    skater.ground.steering.update(tilt, skater.air_settings.steering_blend, p.flags_2468, p.flags_2472);
}

void Runtime::shift_footplant_trajectory_to_index(KnownAirTrajectory& t, std::int32_t index) {
    const auto source = actual(t);
    const float time = float(index) * rs::from_bits(0x3c888889u);
    t.position = source.position_at(time);
    t.velocity = source.velocity_at(time);
}

void Runtime::update_footplant_prediction(const KnownAirFootplantInput& input) {
    const auto g = physics.settings.step.simulation.gravity_acceleration;
    const auto& edges = physics.grind_world->primitives();
    const FootplantFrame f{
        skater.player_input.processed,
        toolkit,
        skater.animated_skeleton,
        skater.skeleton,
        {g.x, g.y, g.z, 0.0f},
        physics.world,
        {edges.data(), edges.size()},
    };
    skater.footplant.update_candidate(input, f);
}

void Runtime::update_footplant_lock(const KnownAirFootplantInput& input) { skater.footplant.update_launch(input); }

void Runtime::update_footplant_pose(const KnownAirFootplantInput& input) {
    const auto g = physics.settings.step.simulation.gravity_acceleration;
    const auto& edges = physics.grind_world->primitives();
    const FootplantFrame f{
        skater.player_input.processed,
        toolkit,
        skater.animated_skeleton,
        skater.skeleton,
        {g.x, g.y, g.z, 0.0f},
        physics.world,
        {edges.data(), edges.size()},
    };
    const auto result = skater.footplant.consume_and_submit(input, f, skater.foot_ik, skater.skeleton_collision);
    record(result);
}

V Runtime::board_body_velocity() {
    const auto v = physics.board.bodies()[6].rates.linear_velocity;
    return {v.x, v.y, v.z, 0.0f};
}

void Runtime::set_board_velocity(V v) {
    for (auto& body : physics.board.bodies_mut()) {
        body.rates.linear_velocity = xyz(v);
    }
}

V Runtime::board_forward_axis() {
    const auto part = physics.board.part_transforms()[6];
    const auto c = part.basis.columns[2];
    return scale({c[0], c[1], c[2], 0.0f},
                 (skater.player_input.processed.flags_2468 & (1u << 20)) != 0 ? -1.0f : 1.0f);
}

void Runtime::check_for_air_wipeout(bool use_com) {
    auto& s = skater;
    const wipeout::Observations observations{
        s.player_input.processed,
        physics.riding.ground,
        s.collision_feedback,
        solve::deck_frame(physics.board),
        s.animated_skeleton.board_frames.animation_target,
        s.animated_skeleton.roots.world_to_animation,
        s.collision_pose_error,
        s.collision_maximum_error,
        s.player_state.post.jump_fix_frames,
        s.air_reckoning.state,
        physics.riding.reckoning_frames.system[1][1],
        s.trajectory.selector.grind_locked_to_middle(),
        s.trajectory.selector.grind_normal(),
    };
    const auto result = s.wipeout.check_air(observations, use_com);
    record(result);
}

AffineTransform Runtime::reckoning_com_transform_816() const {
    AffineTransform t;
    t.vectors = physics.riding.reckoning_frames.system;
    return t;
}

}  // namespace s3::game::physics::known_air::runtime
