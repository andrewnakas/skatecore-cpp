// Port of skate-core/src/player/wipeout_state/output.rs (whole file).
#include "s3/core/player/wipeout_state/output.hpp"

#include "s3/core/player/wipeout_state/math.hpp"
#include "s3/core/player/wipeout_state/orientation.hpp"

namespace s3::player::wipeout_state::data {

output::Output State::output(const physics::skeleton_animation_record::AnimationPartTransform& hips,
                             std::array<float, 4> normal) const {
    const auto right = hips[0];
    const auto projected = math::sub(normal, math::scale(right, math::dot(normal, right)));
    const float right_angle = orientation::signed_angle(right, normal, {0.0f, 0.0f, 0.0f, 0.0f});
    const float up_angle =
        math::wrap_angle(orientation::signed_angle(projected, hips[1], right) + rs::from_bits(0x40490FDBu));
    output::Output o;
    o.over_599 = over;
    o.scalar_544 = orientation;
    o.no_support_time_548 = no_support_time;
    o.can_leave_72 = recovery_eligible && !prevent_manual && !ignore_reset;
    o.special_surface_81 = special_surface;
    o.below_surface_82 = below_surface;
    o.surface_height_32 = surface_height;
    o.surface_height_valid_83 = true;
    o.collision_time_144 = predicted_time;
    o.profile_148 = static_cast<std::uint32_t>(profile);
    o.response_strength_580 = response_scalar;
    o.extra_weight_584 = extra_weight;
    o.time_until_teleport_576 = time_until_teleport;
    o.teleport_pending_604 = teleport_pending;
    o.material_ten_3479 = material_ten_response;
    o.material_eleven_3480 = material_eleven_response;
    o.imminent_surface_twelve_3483 = imminent_surface_twelve;
    o.predicted_position_64 = predicted_position;
    if (retained_velocity_active) o.retained_air_velocity_160 = retained_velocity;
    if (response_finished) o.response_change_588 = response_change;
    o.teleport_countdown_68 = teleport_countdown >= 0;
    o.request_teleport_69 = request_teleport;
    o.hips_right_angle_496 = right_angle;
    o.hips_up_angle_500 = up_angle;
    return o;
}

}  // namespace s3::player::wipeout_state::data
