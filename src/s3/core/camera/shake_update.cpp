// Port of skate-core/src/camera/shake_update.rs.
//! Complete normal-camera ShakeEffect update82E05800.
#include "s3/core/camera/manager_state.hpp"
#include "s3/core/camera/orientation_math.hpp"
#include "s3/core/camera/shake.hpp"
#include "s3/core/camera/shake_curve.hpp"
#include "s3/core/camera/vector_tracker.hpp"

namespace s3::camera {

using manager_state::clamp;
using math::Basis3;

Basis3 ShakeEffect::update(float dt, float speed, float distance, const Basis3& basis, bool enabled,
                           const ShakeSamples& samples, const ShakeSettings& settings) {
    this->weight = clamp(enabled ? rs::mul_add(dt, 5.0f, this->weight) : -rs::mul_add(dt, 5.0f, -this->weight), 0.0f,
                         1.0f);
    float amplitude;
    if (settings.amplitude_top_speed != 0.0f) {
        amplitude = rs::mul_add(
            shake_curve::sample(settings.amplitude_curve, clamp(speed / settings.amplitude_top_speed, 0.0f, 1.0f)),
            settings.amplitude_maximum - settings.amplitude_minimum, settings.amplitude_minimum);
    } else {
        amplitude = settings.amplitude_minimum;
    }
    amplitude = (this->impulse + amplitude) * this->weight;
    float frequency;
    if (settings.frequency_top_speed != 0.0f) {
        frequency = rs::mul_add(
            shake_curve::sample(settings.frequency_curve, clamp(speed / settings.frequency_top_speed, 0.0f, 1.0f)),
            settings.frequency_maximum - settings.frequency_minimum, settings.frequency_minimum);
    } else {
        frequency = settings.frequency_minimum;
    }
    if (this->impulse > 0.0f) {
        frequency = settings.impulse_frequency;
    }
    this->time = rs::mul_add(settings.data_frames_per_second * frequency, dt, this->time);
    const float inverse = vector_tracker::refined_reciprocal(1.0f);
    const float whole = rs::trunc(inverse * this->time);
    const float fraction = rs::mul_add(-whole, 1.0f, this->time);
    const std::size_t count = samples.rotations.size();
    const std::size_t left = std::size_t(rs::as_u32(this->time - fraction)) % count;
    const std::size_t right = (left + 1) % count;
    const float distance_weight =
        clamp((rs::from_bits(0x4059999a) - distance) * rs::from_bits(0x3fd55552), 0.0f, 1.0f);
    const float inverse_fraction = 1.0f - fraction;
    std::array<float, 3> angles = rs::from_fn<3>([&](std::size_t i) {
        const float interpolated =
            rs::mul_add(samples.rotations[left][i], inverse_fraction, samples.rotations[right][i] * fraction);
        return ((interpolated * amplitude) * distance_weight) * settings.rotation_multiplier;
    });
    // vrlimi mask2 selects Z (Dutch/roll), not Y.
    angles[2] *= settings.dutch_multiplier;
    const Basis3 rotation = orientation_math::basis_from_angles(angles[0], angles[1], angles[2]);
    const std::array<float, 3> local_translation = rs::from_fn<3>([&](std::size_t i) {
        const float interpolated =
            rs::mul_add(samples.translations[left][i], inverse_fraction, samples.translations[right][i] * fraction);
        return ((interpolated * settings.translation_multiplier) * amplitude) * distance_weight;
    });
    this->translation = rs::from_fn<4>([&](std::size_t i) {
        if (i == 3) {
            return 0.0f;
        }
        return rs::mul_add(basis.columns[2][i], local_translation[2],
                           rs::mul_add(basis.columns[1][i], local_translation[1],
                                       rs::mul_add(basis.columns[0][i], local_translation[0], 0.0f)));
    });
    Basis3 result;
    result.columns = rs::map(rotation.columns, [&](const std::array<float, 3>& column) {
        return rs::from_fn<3>([&](std::size_t i) {
            return rs::mul_add(column[2], basis.columns[2][i],
                               rs::mul_add(column[1], basis.columns[1][i], column[0] * basis.columns[0][i]));
        });
    });
    this->impulse = (1.0f - settings.impulse_decay) * this->impulse;
    if (this->impulse < rs::from_bits(0x3ba3d70a)) {
        this->impulse = 0.0f;
    }
    return result;
}

}  // namespace s3::camera
