// Port of skate-core/src/physics/grind_forces/reckoning.rs.
#include "s3/core/physics/grind_forces/reckoning.hpp"

namespace s3::physics::grind_forces::reckoning {

using board_motion_output::inverse_length_squared;
using math::Vector3;

namespace {
Vector3 xyz(const V& v) { return Vector3{v[0], v[1], v[2]}; }
V lanes(Vector3 v) { return V{v.x, v.y, v.z, 0.0f}; }
}  // namespace

void update(riding::ground_orientation::GroundOrientation& orientation,
            riding::reckoning_frames::ReckoningFrames& frames, air::body_spin::BodySpinState& body_spin,
            air::reckoning::AirState& air, const Settings& settings, V normal, V heading, float smoothing,
            bool reverse_stance, float physical_body_spin) {
    const V old_up = lanes(orientation.up);
    orientation.dynamic_up = orientation.up;
    orientation.up_velocity = Vector3::ZERO;
    frames.target_lean_angle = 0.0f;
    air.secondary_lean_angle = 0.0f;
    orientation.ground_blend = 0.0f;
    const V ground = orientation.ground_filter.update(settings.ground_normal_smoothing, normal);
    orientation.ground_normal = xyz(ground);
    frames.heading = heading;
    V candidate{};
    for (std::size_t i = 0; i < 4; ++i) {
        candidate[i] = rs::mul_add(old_up[i], smoothing, normal[i] * (1.0f - smoothing));
    }
    // Native NormalizeSafe threshold is the original830BD350 splat1e-6;
    // zero/short candidates retain the preceding up, not world-up.
    V up;
    if (length(candidate) > rs::from_bits(0x358637bdu)) {
        const float inverse = inverse_length_squared(dot3(candidate, candidate), 2);
        up = V{candidate[0] * inverse, candidate[1] * inverse, candidate[2] * inverse, candidate[3] * inverse};
    } else {
        up = old_up;
    }
    orientation.up = xyz(up);
    orientation.target = orientation.up;
    orientation.slow_filter.filter(up);
    orientation.fast_filter.filter(up);
    orientation.slow_filter.publish_current(up);
    orientation.fast_filter.publish_current(up);
    frames.calculate_transform(up, ground);
    frames.calculate_tilt(reverse_stance, settings.tilt_vs_rotation, settings.tilt_vs_slope);
    // Caller82D3F4E8 passes updateBodySpin=false, hence0. Other native callers
    // may supply their actual PhysBodySpin; this is still the ground branch.
    air::body_spin::update_ground(body_spin, physical_body_spin);
    // No ResetSpin: Air's spin_angle/spin_speed survive this function.
}

}  // namespace s3::physics::grind_forces::reckoning
