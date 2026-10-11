// Port of skate-core/src/physics/manual/controller.rs (non-generic helpers).
#include "s3/core/physics/manual/controller.hpp"

namespace s3::physics::manual::controller::detail {

void apply_correction(ManualEffect& effect, const ManualInput& input, ManualMode mode, float target,
                      float measured) {
    const auto& frame = input.velocity_frame_z;
    const auto& velocity = input.angular_velocity_world;
    const float local_z = rs::mul_add(frame[2], velocity[2],
                                      rs::mul_add(frame[1], velocity[1], frame[0] * velocity[0]));
    if (!input.braking || !(0.0f > local_z * input.balance)) {
        return;
    }
    if (!mode.corrective_force_enabled) {
        effect.opposing_motion_without_correction = true;
        return;
    }
    if (!(rs::abs(local_z) > mode.correction_angular_speed_threshold)) {
        return;
    }
    const float radians_to_degrees = rs::from_bits(0x42652ee1u);
    const float error_degrees = (target * radians_to_degrees) - (measured * radians_to_degrees);
    if (!(rs::abs(error_degrees) < 1.0f)) {
        return;
    }
    const bool alternate =
        input.reversed_point_selection ? input.balance < 0.0f : input.balance > 0.0f;
    effect.corrective_point_body =
        alternate ? input.correction_point_7888 : input.correction_point_7952;
    effect.corrective_force_world =
        rs::map(velocity, [](float component) { return (component * -1.0f) * 400.0f; });
    effect.correction_active = true;
}

float clamp_symmetric(float value, float bound) {
    const float lower = -bound;
    const float lower_clamped = lower - value >= 0.0f ? lower : value;
    return bound - lower_clamped >= 0.0f ? lower_clamped : bound;
}

}  // namespace s3::physics::manual::controller::detail
