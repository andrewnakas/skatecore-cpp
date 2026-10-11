// Port of skate-core/src/animation/intent_filter.rs.
//! FilterMotionGraphIntent TU3 82BB1730/17D0/19F0 and 82BB1B78.
//! Coefficients and limits are per graph update; only the ramp uses elapsed time.
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <utility>

#include "s3/core/rs.hpp"

namespace s3::animation::intent_filter {

struct Settings {
    float starting_value = 0.0f;
    float default_value = 0.0f;
    float scale = 0.0f;
    std::array<std::uint32_t, 4> filters{};
    std::optional<float> ramp_time;
    float blend_rising = 0.0f;
    float blend_falling = 0.0f;
    std::optional<float> blend_out;
    std::optional<float> clamp_velocity;
    std::optional<float> clamp_acceleration;
};
S3_DERIVE_EQ(Settings, starting_value, default_value, scale, filters, ramp_time, blend_rising, blend_falling,
             blend_out, clamp_velocity, clamp_acceleration)

struct State {
    float elapsed = 0.0f;
    float previous_delta = 0.0f;
    float value = 0.0f;

    /// Begin publishes startingValue directly, without scaling or filtering.
    float begin(const Settings& settings);

    /// Stance is the live animation component's fakie/mirrored getter pair.
    float update(const Settings& settings, std::optional<float> input, float dt, std::pair<bool, bool> stance);
};
S3_DERIVE_EQ(State, elapsed, previous_delta, value)

float limit_delta(float value, float lower, float upper);
float ramp(float elapsed, float time);

}  // namespace s3::animation::intent_filter
