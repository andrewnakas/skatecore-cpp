// Port of skate-core/src/riding/push.rs.
#include "s3/core/riding/push.hpp"

namespace s3::riding::push {

PushAcceleration calculate_acceleration(PushInput input, PushLimits limits) {
    if ((input.flags_2468 & 0x02000000u) == 0) {
        return PushAcceleration{};
    }
    if ((input.flags_2472 & 0x200u) != 0) {
        PushAcceleration out{};
        out.suppressed = true;
        return out;
    }
    const float gap = input.target_speed - input.current_speed;
    // 82D94930 bgt: unordered and both signed zeros take the zero branch.
    float speed_change = gap > 0.0f ? gap : 0.0f;
    const float speed_fraction = input.current_speed / limits.maximum_pushable_speed;
    // 82D94960/68 are ordered fsel tests, not host clamp. Unordered ratios
    // select the original ratio first, then1 on the upper test.
    const float nonnegative = -speed_fraction >= 0.0f ? 0.0f : speed_fraction;
    const float fraction = 1.0f - nonnegative >= 0.0f ? nonnegative : 1.0f;
    // Skate 3 blends two limits here. Skate 2 uses a single limit.
    const float limit =
        rs::mul_add(1.0f - fraction, limits.low_speed_change, fraction * limits.high_speed_change);
    const bool below_lower = speed_change < -limit;
    if (speed_change > limit) {
        speed_change = limit;
    }
    if (below_lower) {
        speed_change = -limit;
    }
    if (input.absolute_body_speed + speed_change > limits.maximum_pushable_speed) {
        const float remaining = limits.maximum_pushable_speed - input.absolute_body_speed;
        speed_change = remaining >= 0.0f ? remaining : 0.0f;
    }
    const float force_magnitude = (input.scale * speed_change) / input.delta_seconds;
    PushAcceleration out{};
    out.vector = Vector3{input.direction.x * force_magnitude, input.direction.y * force_magnitude,
                         input.direction.z * force_magnitude};
    return out;
}

PushAcceleration enqueue_push(PushInput input, PushLimits limits, physics::Slice<const float> inverse_masses,
                              BoardForceQueue& queue) {
    input.scale = physics::force_queue::total_body_mass(inverse_masses);
    const PushAcceleration output = calculate_acceleration(input, limits);
    queue.append(QueuedPointForce{3, output.vector, output.local_point});
    return output;
}

}  // namespace s3::riding::push
