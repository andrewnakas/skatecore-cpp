// Port of skate-core/src/player/wipeout_state/control_profile_air.rs (whole file).
#include "s3/core/player/wipeout_state/control_profile_air.hpp"

#include "s3/core/player/wipeout_state/orientation.hpp"
#include "s3/core/player/wipeout_state/torque.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::player::wipeout_state::control_profile_air {

using math::V;
using physics::skeleton_animation_record::AnimationPartTransform;

namespace {
const V UP{0.0f, 1.0f, 0.0f, 0.0f};

float retain_input(float input, float previous, float inertia) {
    if (rs::abs(input) >= rs::abs(previous)) {
        return input;
    }
    return (1.0f - inertia) * input + previous * inertia;
}

V transform(const AnimationPartTransform& frame, V vector) {
    return math::madd(frame[2], vector[2], math::madd(frame[1], vector[1], math::scale(frame[0], vector[0])));
}

V horizontal_alignment(const profiles::Profile& profile, const AnimationPartTransform& effective, V direction,
                       V tilt) {
    const V local = math::normalize_or(profile.horizontal_axis, V{1.0f, 0.0f, 0.0f, 0.0f});
    const V world = transform(effective, local);
    V target = orientation::rotate(tilt, math::normalize_or(math::cross(direction, UP), V{}));
    if (!profile.horizontal_directed) {
        target = math::scale(target, math::dot(world, target) > 0.0f ? 1.0f : -1.0f);
    }
    // 82D3D4F0 uses the normalized authored axis here, before world composition;
    // the angle query itself receives world/target. Preserve that distinction.
    const V axis = math::normalize_or(math::cross(local, target), V{});
    if (!(math::dot(axis, axis) > 0.9f)) {
        return V{};
    }
    const float angle = math::wrap_angle(orientation::projected_angle(world, target, axis));
    const float limit = rs::from_bits(0x3F490FDBu);
    return math::scale(axis, math::clamp(angle, -limit, limit) * 0.8f);
}
}  // namespace

void update(State& state, physics::skeleton_body::SkeletonBody& body, V physical_com,
            const AnimationPartTransform& effective, std::array<float, 2> input, const profiles::Profile& profile) {
    state.angular_velocity = torque::angular_velocity(body.record, body.definition.animation_masses.fractional);
    const float radians = rs::from_bits(0x3C8EFA35u);
    const V tilt_input{(input[1] * profile.tilt_degrees) * radians, 0.0f,
                       (input[0] * profile.tilt_degrees) * -radians, 0.0f};
    state.retained_tilt = math::madd(tilt_input, 0.05f, math::scale(state.retained_tilt, 0.95f));
    const float tilt_length = math::length(state.retained_tilt);
    V tilt_rotation{0.0f, 0.0f, 0.0f, 1.0f};
    if (tilt_length > 0.01f) {
        const auto [sin, cos] = trigonometry::sin_cos(tilt_length * 0.5f);
        tilt_rotation = math::scale(math::scale(state.retained_tilt, math::reciprocal(tilt_length)), sin);
        tilt_rotation[3] = cos;
    }
    const V desired = transform(effective, orientation::alignment_up(profile.align_euler[0], profile.align_euler[1]));
    const V side = math::normalize_or(state.right, V{});
    const V projected_velocity = math::sub(state.velocity, math::scale(side, math::dot(state.velocity, side)));
    const V direction = math::dot(state.forward, projected_velocity) <= 0.0f
                            ? math::normalize(math::add(desired, V{0.0f, -100.0f, 0.0f, 0.0f}))
                            : math::normalize_or(projected_velocity, desired);
    const V tilted_direction = orientation::rotate(tilt_rotation, direction);
    const V axis = math::normalize_or(math::cross(tilted_direction, desired), desired);
    const float angle = math::wrap_angle(orientation::signed_angle(tilted_direction, desired, axis));
    const V distance_torque =
        profile.align_with_velocity ? math::scale(math::scale(axis, angle), profile.torque_distance) : V{};

    const V input_side_axis = orientation::rotate(tilt_rotation, desired);
    const V input_forward_axis =
        orientation::rotate(tilt_rotation, math::normalize_or(math::cross(direction, UP), V{}));
    state.retained_sideways_input =
        retain_input(state.sideways_input, state.retained_sideways_input, profile.spin_inertia);
    state.retained_forward_input = retain_input(state.forward_input, state.retained_forward_input, profile.spin_inertia);
    const float side_spin = profile.sideways_spin * state.retained_sideways_input;
    const float forward_spin = profile.forward_spin * state.retained_forward_input;
    V spin = math::madd(input_forward_axis, forward_spin, math::madd(input_side_axis, side_spin, V{}));
    if (math::dot(spin, state.angular_velocity) <= 0.0f) {
        state.control_time = 0.0f;
    }
    spin = math::scale(spin, profile.spin_vs_time.evaluate(state.control_time));
    const V horizontal =
        rs::abs(side_spin) <= 0.1f ? horizontal_alignment(profile, effective, direction, tilt_rotation) : V{};
    const V control = math::add(
        math::madd(math::sub(spin, state.angular_velocity), profile.torque_velocity, distance_torque), horizontal);
    torque::apply(body, physical_com, control);
}

}  // namespace s3::player::wipeout_state::control_profile_air
