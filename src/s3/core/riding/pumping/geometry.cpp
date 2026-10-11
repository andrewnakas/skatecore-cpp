// Port of skate-core/src/riding/pumping/geometry.rs.
#include "s3/core/riding/pumping/geometry.hpp"

#include "s3/core/physics/board_motion_output.hpp"
#include "s3/core/physics/native_arithmetic.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::riding::pumping::geometry {

using physics::board_motion_output::inverse_length_squared;
using physics::native_arithmetic::dot3;
using physics::native_arithmetic::reciprocal_estimate;

namespace {

float reciprocal(float value) {
    float inverse = reciprocal_estimate(value);
    for (int n = 0; n < 2; ++n) {
        const float error = rs::mul_add(-inverse, value, 1.0f);
        inverse = rs::mul_add(inverse, error, inverse);
    }
    return inverse;
}

std::array<float, 4> cross(const std::array<float, 4>& a, const std::array<float, 4>& b) {
    const std::array<std::size_t, 4> cyclic{1, 2, 0, 3};
    std::array<float, 4> product{};
    for (std::size_t i = 0; i < 4; ++i) product[i] = rs::mul_add(-a[cyclic[i]], b[i], a[i] * b[cyclic[i]]);
    std::array<float, 4> out{};
    for (std::size_t i = 0; i < 4; ++i) out[i] = product[cyclic[i]];
    return out;
}

}  // namespace

rs::Result<float, Infallible> NativePumpingGeometry::height(std::array<float, 4> normal, std::array<float, 4> com) {
    return dot3(normal, com);
}

rs::Result<float, Infallible> NativePumpingGeometry::speed(std::array<float, 4> previous,
                                                           std::array<float, 4> current, float dt) {
    const float inverse = reciprocal(dt);
    std::array<float, 4> velocity{};
    for (std::size_t i = 0; i < 4; ++i) velocity[i] = (current[i] - previous[i]) * inverse;
    const float squared = dot3(velocity, velocity);
    const float length = squared * inverse_length_squared(squared, 2);
    return squared == 0.0f ? 0.0f : length;
}

rs::Result<float, Infallible> NativePumpingGeometry::angular_speed(std::array<float, 4> previous_position,
                                                                   std::array<float, 4> previous_normal,
                                                                   const PumpingSample& sample, float dt) {
    std::array<float, 4> displacement{};
    for (std::size_t i = 0; i < 4; ++i) displacement[i] = sample.position[i] - previous_position[i];
    std::array<float, 4> axis = cross(displacement, sample.normal);
    const float squared = dot3(axis, axis);
    const float inverse = inverse_length_squared(squared, 2);
    const float length = squared == 0.0f ? 0.0f : squared * inverse;
    if (length > rs::from_bits(0x358637bdu)) {
        axis = rs::map(axis, [inverse](float v) { return v * inverse; });
    } else {
        axis = {0.0f, 0.0f, 0.0f, 0.0f};
    }
    const float inverse_dt = reciprocal(dt);
    const auto angular = rs::map(cross(previous_normal, sample.normal), [inverse_dt](float v) { return v * inverse_dt; });
    return dot3(angular, axis);
}

rs::Result<float, Infallible> NativePumpingGeometry::inclination_radians(float y) { return trigonometry::acos(y); }

}  // namespace s3::riding::pumping::geometry
