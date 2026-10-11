// Port of skate-core/src/riding/anti_flip.rs.
#include "s3/core/riding/anti_flip.hpp"

#include "s3/core/riding/vector.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::riding::anti_flip {

using vector::cross;
using vector::dot3;
using vector::normalize;

std::array<float, 4> calculate(const AntiFlipSettings& s, const AntiFlipInput& i) {
    if ((i.flags_2468 & 0x20000000u) != 0) {
        return {0.0f, 0.0f, 0.0f, 0.0f};
    }
    const auto project = [&](const std::array<float, 4>& axis) {
        const float dot = dot3(i.projection_axis_544, axis);
        std::array<float, 4> v{};
        for (std::size_t n = 0; n < 4; ++n) v[n] = axis[n] - i.projection_axis_544[n] * dot;
        return normalize(v, s.normal_threshold);
    };
    const auto a = project(i.axis_96);
    const auto b = project(i.axis_64);
    const auto angle = [](const std::array<float, 4>& axis, const std::array<float, 4>& projected) {
        float d = dot3(axis, projected);
        d = 0.0f > d ? 0.0f : d;
        d = 1.0f < d ? 1.0f : d;
        return trigonometry::acos(d) * rs::from_bits(0x3f22f983u);
    };
    const float sign_a = dot3(i.axis_64, cross(i.axis_96, a)) >= 0.0f ? 1.0f : -1.0f;
    const float sign_b = dot3(i.axis_96, cross(i.axis_64, b)) >= 0.0f ? 1.0f : -1.0f;
    const float b_strength = s.axis_64_response.evaluate(angle(i.axis_64, b)) * sign_b;
    const float a_strength = (s.axis_96_response.evaluate(angle(i.axis_96, a)) * sign_a) *
                             (i.balance == 0.0f ? 1.0f : 0.0f);
    std::array<float, 4> out{};
    for (std::size_t n = 0; n < 4; ++n) out[n] = rs::mul_add(i.axis_64[n], a_strength, i.axis_96[n] * b_strength);
    return out;
}

}  // namespace s3::riding::anti_flip
