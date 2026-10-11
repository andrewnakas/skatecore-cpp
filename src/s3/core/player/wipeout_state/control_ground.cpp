// Port of skate-core/src/player/wipeout_state/control_ground.rs (whole file).
#include "s3/core/player/wipeout_state/control_ground.hpp"

#include "s3/core/player/wipeout_state/torque.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::player::wipeout_state::control_ground {

using math::V;
using physics::skeleton_animation_record::AnimationPartTransform;

void update(State& state, physics::skeleton_body::SkeletonBody& body, V physical_com,
            const AnimationPartTransform& effective, V ground_axis_464, const profiles::Profile& profile,
            const AnimationPartTransform& animation_to_world,
            const std::array<AnimationPartTransform, 24>& animation_pose) {
    state.angular_velocity = torque::angular_velocity(body.record, body.definition.animation_masses.fractional);
    const float speed = math::length(state.velocity);
    V rolling_axis = math::normalize_or(rs::from_fn<4>([&](std::size_t i) {
                                            const float x = effective[0][i] * profile.roll_axis[0];
                                            const float y = rs::mul_add(effective[1][i], profile.roll_axis[1], x);
                                            return rs::mul_add(effective[2][i], profile.roll_axis[2], y);
                                        }),
                                        V{});
    const std::array<std::size_t, 2> hand_parts{3, 7};
    const auto hands = rs::map(hand_parts, [&](std::size_t i) {
        return physics::skeleton_animation_record::compose_affine(animation_to_world, animation_pose[i]);
    });
    const bool hands_close = math::length(math::sub(hands[0][3], hands[1][3])) <= 0.6f;
    V alignment{};
    if (profile.align_ground_with_velocity) {
        const V axis = math::normalize_or(math::cross(ground_axis_464, state.velocity), V{});
        if (math::dot(rolling_axis, axis) < 0.0f) {
            rolling_axis = math::scale(rolling_axis, -1.0f);
        }
        const V perpendicular = math::cross(rolling_axis, axis);
        const float sine = math::clamp(math::length(perpendicular), -0.9999f, 0.9999f);
        const float angle = math::wrap_angle(trigonometry::asin(sine));
        const float magnitude = math::clamp(angle * 1.2f, 0.0f, 1.0f);
        if (speed > 2.0f) {
            const V axis2 = math::normalize_or(perpendicular, V{});
            const V current = math::normalize_or(state.angular_velocity, V{});
            alignment = math::scale(
                math::sub(math::scale(axis2, magnitude), math::scale(axis2, math::dot(current, axis2))),
                profile.ground_align_torque);
        }
    }
    V rolling{};
    if (hands_close && profile.roll_on_ground && speed > 2.0f) {
        const V current = math::normalize_or(state.angular_velocity, V{});
        rolling = math::scale(math::sub(math::scale(math::scale(rolling_axis, 1.5f), state.forward_input),
                                        math::scale(rolling_axis, math::dot(current, rolling_axis))),
                              profile.ground_roll_torque);
    }
    torque::apply(body, physical_com, math::add(alignment, rolling));
}

}  // namespace s3::player::wipeout_state::control_ground
