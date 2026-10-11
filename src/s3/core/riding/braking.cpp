// Port of skate-core/src/riding/braking.rs.
#include "s3/core/riding/braking.hpp"

namespace s3::riding::braking {

QueuedPointForce calculate_braking(BrakeInput input, BrakeSettings settings) {
    float amount = (input.flags_2468 & 0x40000000u) != 0 ? input.input_2728 * settings.input_force : 0.0f;
    if ((input.flags_2468 & 0x20000000u) != 0) {
        amount = settings.override_force;
    }
    const float scaled = input.surface_factor * amount;
    const float sign = input.signed_speed < 0.0f ? 1.0f : -1.0f;
    amount = sign * scaled;
    // 82D94B78 bge retains the amount when the comparison is unordered.
    if (input.absolute_body_speed < settings.minimum_speed) {
        amount = 0.0f;
    }
    return QueuedPointForce{
        2,
        Vector3{input.direction.x * amount, input.direction.y * amount, input.direction.z * amount},
        Vector3::ZERO,
    };
}

float calculate_linear_drag(LinearDragInput input, LinearDragSettings settings) {
    const bool brake_override = input.absolute_body_speed < settings.brake_speed * 2.0f &&
                                (input.flags_2468 & 0x60000000u) != 0 && !(input.scalar_2724 > 0.0f) &&
                                input.balance_2720 == 0.0f;
    const bool balance_drag = input.balance_2720 != 0.0f && input.absolute_body_speed < settings.balance_speed &&
                              input.comparison_scalar > settings.comparison_threshold;
    if (brake_override) {
        return 1.0f;
    } else if (balance_drag) {
        return settings.balance_drag;
    }
    return 0.0f;
}

}  // namespace s3::riding::braking
