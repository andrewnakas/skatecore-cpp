// Port of skate-core/src/camera/look_input.rs.
//! Stock gameplay FreeCamManager82E084A0, used by a shot's look-stick option.
//! This is the normal camera's offset filter, unrelated to the replay editor.
#pragma once
#include <array>

#include "s3/core/rs.hpp"

namespace s3::camera {

struct LookSettings {
    float heading_offset_degrees = 0.0f;
    float heading_speed = 0.0f;
    float elevation_offset_degrees = 0.0f;
    float elevation_speed = 0.0f;
};
S3_DERIVE_EQ(LookSettings, heading_offset_degrees, heading_speed, elevation_offset_degrees, elevation_speed)

struct LookInput {
    float elevation = 0.0f;
    float heading = 0.0f;

    static LookInput new_() { return LookInput{0.0f, 0.0f}; }

    void update(std::array<float, 2> input, LookSettings settings) {
        const float radians = rs::from_bits(0x3c8efa35);
        const float limit = rs::from_bits(0x3fc90fdb) - 5.0f * radians;
        auto clamp = [&](float value) {
            const float lower = -limit - value >= 0.0f ? -limit : value;
            return limit - lower >= 0.0f ? lower : limit;
        };
        const float heading_ = clamp((input[0] * settings.heading_offset_degrees) * radians);
        this->heading =
            rs::mul_add(heading_ - this->heading, settings.heading_speed * rs::from_bits(0x3c888889), this->heading);
        const float elevation_ = clamp((input[1] * settings.elevation_offset_degrees) * radians);
        this->elevation = rs::mul_add(elevation_ - this->elevation,
                                      settings.elevation_speed * rs::from_bits(0x3c888889), this->elevation);
    }
};
S3_DERIVE_EQ(LookInput, elevation, heading)

}  // namespace s3::camera
