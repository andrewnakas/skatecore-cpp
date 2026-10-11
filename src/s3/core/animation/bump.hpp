// Port of skate-core/src/animation/bump.rs.
//! SetBumpCoefficients Begin82BB0660, ctor82BC7318/vtable823200C8+48.
#pragma once
#include <array>

#include "s3/core/physics/native_arithmetic.hpp"
#include "s3/core/rs.hpp"

namespace s3::animation::bump {

struct Settings {
    float scale_x_acc = 0.0f;
    float min_bump_mag = 0.0f;
    float min_bump_blend_value = 0.0f;
    float max_bump_mag = 0.0f;
};

///Input is PhysOutAnimation112 (already ground-conditioned), not raw world acceleration.
inline std::array<float, 2> coefficients(std::array<float, 4> acceleration, bool mirrored, const Settings& settings) {
    namespace na = physics::native_arithmetic;
    acceleration[0] *= settings.scale_x_acc;
    const float squared = na::dot3(acceleration, acceleration);
    float inverse = na::reciprocal_square_root_estimate(squared);
    for (int i = 0; i < 2; ++i) {
        inverse = rs::mul_add(inverse * 0.5f, rs::mul_add(-squared, inverse * inverse, 1.0f), inverse);
    }
    const float magnitude = squared == 0.0f ? 0.0f : squared * inverse;
    const float t = rs::clamp((magnitude - settings.min_bump_mag) / (settings.max_bump_mag - settings.min_bump_mag),
                              0.0f, 1.0f);
    const float weight = rs::mul_add(t, 1.0f - settings.min_bump_blend_value, settings.min_bump_blend_value);
    //Initializer82F826F8 broadcasts82181A88 into830BD350: length threshold1e-6.
    const std::array<float, 2> direction = magnitude > rs::from_bits(0x358637bd)
                                               ? std::array<float, 2>{acceleration[0] * inverse,
                                                                      acceleration[2] * inverse}
                                               : std::array<float, 2>{0.0f, 0.0f};
    return rs::map(direction, [&](float v) {
        const float value = v * weight;
        return mirrored ? -value : value;
    });
}

}  // namespace s3::animation::bump
