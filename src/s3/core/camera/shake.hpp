// Port of skate-core/src/camera/shake.rs (+ shake_update.rs: ShakeEffect::update).
//! Stock normal-camera shake data and impact response82E05DC0/82E055D8.
#pragma once
#include <array>
#include <string>
#include <vector>

#include "s3/core/camera/_types.hpp"
#include "s3/core/math.hpp"

namespace s3::camera {

struct ShakeSamples {
    std::vector<F4> rotations;     // pub(super)
    std::vector<F4> translations;  // pub(super)

    /// Eight floats per .shk record. Native consumes XYZ of both records and
    /// discards each fourth token; it subtracts each complete sequence's mean.
    static rs::Result<ShakeSamples, std::string> from_rows(const std::vector<std::array<float, 8>>& rows);
};
S3_DERIVE_EQ(ShakeSamples, rotations, translations)

struct ShakeSettings {
    F4x4 amplitude_curve{};               // parameter0
    F4x4 frequency_curve{};               // 64
    float impulse_magnitude = 0.0f;       // 744
    float impulse_minimum_velocity = 0.0f;  // 748
    float impulse_maximum_velocity = 0.0f;  // 752
    float impulse_frequency = 0.0f;       // 756
    float impulse_decay = 0.0f;           // 760
    float amplitude_minimum = 0.0f;       // 764
    float amplitude_maximum = 0.0f;       // 768
    float amplitude_top_speed = 0.0f;     // 772
    float frequency_minimum = 0.0f;       // 776
    float frequency_maximum = 0.0f;       // 780
    float frequency_top_speed = 0.0f;     // 784
    float data_frames_per_second = 0.0f;  // 788
    float translation_multiplier = 0.0f;  // 792
    float rotation_multiplier = 0.0f;     // 796
    float dutch_multiplier = 0.0f;        // 800
};
S3_DERIVE_EQ(ShakeSettings, amplitude_curve, frequency_curve, impulse_magnitude, impulse_minimum_velocity,
             impulse_maximum_velocity, impulse_frequency, impulse_decay, amplitude_minimum, amplitude_maximum,
             amplitude_top_speed, frequency_minimum, frequency_maximum, frequency_top_speed, data_frames_per_second,
             translation_multiplier, rotation_multiplier, dutch_multiplier)

struct ShakeEffect {
    F4 translation{};
    float time = 0.0f;
    float impulse = 0.0f;
    float weight = 0.0f;

    static ShakeEffect new_() { return ShakeEffect{}; }

    ///82E055D8, including grazing-angle attenuation before velocity shaping.
    bool landing_impulse(F4 normal, F4 velocity, const ShakeSettings& settings);

    /// shake_update.rs: complete normal-camera ShakeEffect update82E05800.
    math::Basis3 update(float dt, float speed, float distance, const math::Basis3& basis, bool enabled,
                        const ShakeSamples& samples, const ShakeSettings& settings);
};
S3_DERIVE_EQ(ShakeEffect, translation, time, impulse, weight)

}  // namespace s3::camera
