// Port of skate-core/src/player/offboard/contact_correction.rs (whole file).
// Biped contact correction82D7C9E0, called before slide/movement/cadence.
// Input displacement vectors come from actual skeleton collision output.
#pragma once
#include <array>

#include "s3/core/physics/native_arithmetic.hpp"
#include "s3/core/physics/reciprocal_sqrt.hpp"
#include "s3/core/rs.hpp"

namespace s3::player::offboard::contact_correction {

using Vector = std::array<float, 4>;

struct Input {
    /// Ground job192/208 from skeleton16288/16304.
    std::array<Vector, 2> collision_displacements{};
    Vector projection_axis_416{};
    Vector up_axis_16{};
};

struct ContactCorrection {
    /// Biped708, copied by common ProcessOutput into OffBoard333.
    bool active = false;
    /// Biped400, copied into OffBoard288 even when inactive; retain old value.
    Vector direction{};
    /// Biped384, recomputed every ground update.
    Vector displacement{};

    void update(const Input& input);
};
S3_DERIVE_EQ(ContactCorrection, active, direction, displacement)

// pub(super)
inline float inverse_length(float squared) {
    float inverse = physics::reciprocal_sqrt::estimate(squared);
    for (int i = 0; i < 2; ++i) {
        inverse = rs::mul_add(inverse * 0.5f, rs::mul_add(-squared, inverse * inverse, 1.0f), inverse);
    }
    return inverse;
}
// pub(super)
inline float magnitude(float squared) {
    const float length = squared * inverse_length(squared);
    return squared == 0.0f ? 0.0f : length;
}
/// Original82BD3D90, preserving four lanes and two reciprocal refinements. pub(super)
inline Vector clamp_length(Vector vector, float maximum) {
    const float length = magnitude(physics::native_arithmetic::dot3(vector, vector));
    if (!(length >= rs::from_bits(0x37800000u))) {
        return vector;
    }
    const float bounded = maximum - length >= 0.0f ? length : maximum;
    float inverse = physics::native_arithmetic::reciprocal_estimate(length);
    for (int i = 0; i < 2; ++i) {
        inverse = rs::mul_add(inverse, rs::mul_add(-inverse, length, 1.0f), inverse);
    }
    return rs::map(vector, [&](float value) { return (value * bounded) * inverse; });
}

}  // namespace s3::player::offboard::contact_correction
