// Port of skate-core/src/riding/straighten.rs.
#include "s3/core/riding/straighten.hpp"

#include "s3/core/riding/vector.hpp"

namespace s3::riding::straighten {

using vector::clamp;
using vector::cross;
using vector::dot3;

std::array<float, 4> calculate(const StraightenSettings& s, const StraightenInput& i) {
    const float limit = rs::mul_add(1.0f - s.time_scalar, i.scalar_2764, s.time_scalar) * s.heading_time_limit;
    const float strength = s.time_response.evaluate(clamp(i.heading_time, 0.0f, limit) / limit) * s.strength;
    std::array<float, 4> forward = i.forward;
    if (dot3(i.forward, i.velocity) < 0.0f) {
        forward = rs::map(i.forward, [](float v) { return -v; });
    }
    const float error = dot3(cross(forward, i.velocity), i.normal);
    const float k = error * strength;
    std::array<float, 4> output = rs::map(i.normal, [k](float v) { return v * k; });
    if (i.turn * error < 0.0f) {
        const float scale = 1.0f - clamp(rs::abs(i.turn), 0.0f, s.opposite_turn_limit) / s.opposite_turn_limit;
        output = rs::map(output, [scale](float v) { return v * scale; });
    }
    return output;
}

}  // namespace s3::riding::straighten
