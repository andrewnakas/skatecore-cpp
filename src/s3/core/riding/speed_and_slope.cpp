// Port of skate-core/src/riding/speed_and_slope.rs.
#include "s3/core/riding/speed_and_slope.hpp"

#include "s3/core/physics/native_arithmetic.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::riding::speed_and_slope {

namespace native_arithmetic = physics::native_arithmetic;

float SpeedAndSlopeSettings::calculate(float ground_normal_y, float forward_speed) const {
    const float normal_y =
        native_arithmetic::vector_min(1.0f, native_arithmetic::vector_max(-1.0f, ground_normal_y));
    const float half_pi = rs::from_bits(0x3FC90FDBu);
    float inverse_half_pi = native_arithmetic::reciprocal_estimate(half_pi);
    for (int n = 0; n < 2; ++n) {
        const float error = rs::mul_add(-inverse_half_pi, half_pi, 1.0f);
        inverse_half_pi = rs::mul_add(inverse_half_pi, error, inverse_half_pi);
    }
    float slope = inverse_half_pi * trigonometry::acos(normal_y);
    if (slope > rs::from_bits(0x3F99999Au)) {
        slope = 0.0f;
    }

    const float absolute_speed = rs::abs(forward_speed);
    const float nonnegative_speed = -absolute_speed >= -0.0f ? 0.0f : absolute_speed;
    const float bounded_speed =
        heading_adjust_max_speed - nonnegative_speed >= -0.0f ? nonnegative_speed : heading_adjust_max_speed;
    const float speed_fraction = bounded_speed / heading_adjust_max_speed;
    const float speed_factor = turn_torque_vs_speed.evaluate(speed_fraction);
    const float slope_factor = turn_torque_vs_slope.evaluate(slope);
    return speed_factor * slope_factor;
}

}  // namespace s3::riding::speed_and_slope
