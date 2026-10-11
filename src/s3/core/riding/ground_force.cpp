// Port of skate-core/src/riding/ground_force.rs.
#include "s3/core/riding/ground_force.hpp"

#include "s3/core/riding/vector.hpp"

namespace s3::riding::ground_force {

using vector::clamp;
using vector::dot3;
using vector::normalize;

std::array<float, 8> calculate(const GroundForceSettings& s, const GroundForceInput& i) {
    const float fraction = clamp(i.argument_1 / s.range_1220, 0.0f, 1.0f);
    // Native double literals at822F8700/+8 are0 and1, narrowed at zero arg1.
    const float zero_scalar = i.argument_1 != 0.0f || i.balance != 0.0f ? 1.0f : 0.0f;
    const float speed = (i.surface_speed * 0.125f) * s.speed_scale_1224;
    const float remainder = ((1.0f - fraction) * s.scale_1228) * zero_scalar;
    const float magnitude = -(rs::mul_add(speed, fraction, remainder) * i.argument_4);
    std::array<float, 4> force{};
    for (std::size_t n = 0; n < 4; ++n) {
        force[n] = rs::mul_add(i.axis_544[n], magnitude, (-i.axis_384[n]) * i.argument_3);
    }
    const auto direction = normalize(i.velocity_400, s.normal_threshold);
    const float projected = dot3(direction, force);
    std::array<float, 8> result{};
    for (std::size_t n = 0; n < 4; ++n) result[n] = force[n] - direction[n] * projected;
    result[6] = i.application_z;
    return result;
}

}  // namespace s3::riding::ground_force
