// Port of skate-core/src/camera/shake.rs.
#include "s3/core/camera/shake.hpp"

#include "s3/core/camera/manager_state.hpp"
#include "s3/core/camera/vector_tracker.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::camera {

using manager_state::clamp;
using vector_tracker::dot;
using vector_tracker::length;
using vector_tracker::refined_reciprocal;

rs::Result<ShakeSamples, std::string> ShakeSamples::from_rows(const std::vector<std::array<float, 8>>& rows) {
    if (rows.empty()) {
        return rs::make_err(std::string("Stock camera shake has no samples"));
    }
    for (const auto& row : rows) {
        for (float v : row) {
            if (!rs::is_finite(v)) {
                return rs::make_err(std::string("Stock camera shake contains a non-finite sample"));
            }
        }
    }
    ShakeSamples out;
    out.rotations.reserve(rows.size());
    out.translations.reserve(rows.size());
    F4 rotation_sum = {0.0f, 0.0f, 0.0f, 0.0f};
    F4 translation_sum = {0.0f, 0.0f, 0.0f, 0.0f};
    for (const auto& row : rows) {
        const F4 rotation = {row[0], row[1], row[2], 0.0f};
        const F4 translation = {row[4], row[5], row[6], 0.0f};
        rotation_sum = rs::from_fn<4>([&](std::size_t i) { return rotation_sum[i] + rotation[i]; });
        translation_sum = rs::from_fn<4>([&](std::size_t i) { return translation_sum[i] + translation[i]; });
        out.rotations.push_back(rotation);
        out.translations.push_back(translation);
    }
    const float inverse = 1.0f / float(rows.size());
    const F4 rotation_mean = rs::map(rotation_sum, [&](float v) { return v * inverse; });
    const F4 translation_mean = rs::map(translation_sum, [&](float v) { return v * inverse; });
    for (std::size_t k = 0; k < out.rotations.size(); ++k) {
        F4& rotation = out.rotations[k];
        F4& translation = out.translations[k];
        rotation = rs::from_fn<4>([&](std::size_t i) { return rotation[i] - rotation_mean[i]; });
        translation = rs::from_fn<4>([&](std::size_t i) { return translation[i] - translation_mean[i]; });
    }
    return out;
}

bool ShakeEffect::landing_impulse(F4 normal, F4 velocity, const ShakeSettings& settings) {
    const float speed = length(velocity);
    if (rs::from_bits(0x3727c5ac) > speed) {
        return false;
    }
    const float inverse = refined_reciprocal(speed);
    const F4 direction = rs::map(velocity, [&](float v) { return inverse * v; });
    const float cosine = clamp(dot(rs::map(normal, [](float v) { return -v; }), direction), -1.0f, 1.0f);
    const float angle = trigonometry::acos(cosine) * rs::from_bits(0x3f22f983);
    const float weighted_speed = speed * (1.0f - clamp(angle, 0.0f, 1.0f));
    if (weighted_speed <= 0.0f) {
        return false;
    }
    const float fraction =
        clamp((weighted_speed - settings.impulse_minimum_velocity) /
                  (settings.impulse_maximum_velocity - settings.impulse_minimum_velocity),
              0.0f, 1.0f);
    const float impulse_ = (fraction * fraction) * settings.impulse_magnitude;
    if (impulse_ > this->impulse) {
        this->impulse = impulse_;
        return true;
    }
    return false;
}

}  // namespace s3::camera
