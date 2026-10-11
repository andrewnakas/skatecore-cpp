// Port of skate-core/src/air/body_flip.rs (whole file).
// TU3 Reckoning::UpdateBodyFlip 82D8EC70, with explicit attribute-read inputs.
// Full scalar gates, spin matrix and conditional combined-matrix publication.
#pragma once
#include <array>
#include <optional>

#include "s3/core/rs.hpp"

namespace s3::air::body_flip {

using Matrix = std::array<std::array<float, 4>, 4>;

struct BodyFlipState {
    /// Reckoning +1584, +1588, +1592.
    float angle = 0.0f;
    float speed = 0.0f;
    float requested_speed = 0.0f;
    /// Reckoning +1072 and +1008. These are distinct native output buffers.
    Matrix spin_transform{};
    Matrix combined_transform{};
};
S3_DERIVE_EQ(BodyFlipState, angle, speed, requested_speed, spin_transform, combined_transform)

/// Actual Attrib::GetAttributePointer results (82B72420), not guessed settings.
/// Missing collection/attribute resolves to the supplied live 830D0850 value.
struct BodyFlipSettings {
    /// Hash 26E7322CCDC3FE23.
    std::optional<float> smoothing;
    /// Hash 3D95C06DC6E3A002.
    std::optional<float> maximum_speed;
    /// Hash 89763E47712871C3.
    std::optional<float> spin_scale;
    float missing_attribute_value = 0.0f;
};

struct BodyFlipInput {
    float requested_speed = 0.0f;
    /// Reckoning +1568, +1152 and +1232; caller-provided axes, without repair.
    float spin_angle = 0.0f;
    std::array<float, 4> normal{};
    std::array<float, 4> flip_axis{};
    /// Processed +2604; only the non-perfect branch uses this dt.
    float timestep = 0.0f;
    /// Processed +2548 -> layout +4 -> byte +28.
    bool perfect_body_flips = false;
};

void update(BodyFlipState& state, const BodyFlipSettings& settings, const BodyFlipInput& input);

}  // namespace s3::air::body_flip
