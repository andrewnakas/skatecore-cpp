// Port of skate-core/src/riding/slide_friction.rs.
#include "s3/core/riding/slide_friction.hpp"

#include "s3/core/riding/vector.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::riding::slide_friction {

using vector::clamp;
using vector::dot3;

std::array<float, 8> calculate(const SlideFrictionSettings& s, const SlideFrictionInput& i) {
    // VMX min/max select the second argument on unordered comparisons.
    float y = -1.0f > i.normal[1] ? -1.0f : i.normal[1];
    y = 1.0f < y ? 1.0f : y;
    float angle = clamp(trigonometry::acos(y) * rs::from_bits(0x3f22f983u), 0.0f, 1.0f);
    angle = 1.0f - s.angle_response.evaluate(angle);
    const float speed = 1.0f - s.speed_response.evaluate(i.surface_speed * 0.1f);
    const float time =
        s.time_response.evaluate(clamp(i.heading_time, 0.0f, s.heading_time_limit) / s.heading_time_limit);
    const float blend = rs::mul_add(1.0f - s.scalar_516, i.scalar_2764, s.scalar_516);
    const float response = angle - speed >= 0.0f ? angle : speed;
    const float scalar = -(((time * blend) * response) * s.friction);
    const float normal_speed = dot3(i.velocity, i.normal);
    std::array<float, 4> tangent{};
    for (std::size_t n = 0; n < 4; ++n) tangent[n] = i.velocity[n] - i.normal[n] * normal_speed;
    const float side_speed = dot3(tangent, i.side_axis);
    std::array<float, 8> output{};
    for (std::size_t n = 0; n < 4; ++n) output[n] = (i.side_axis[n] * side_speed) * scalar;
    return output;
}

}  // namespace s3::riding::slide_friction
