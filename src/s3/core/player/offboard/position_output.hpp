// Port of skate-core/src/player/offboard/position_output.rs (whole file).
// Biped frame/position output stages82D80360 and82D80548.
// The retained state is part of the actual Biped owner; no inferred initialization.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/rs.hpp"

namespace s3::player::offboard::position_output {

using Vector = std::array<float, 4>;
using Matrix = std::array<Vector, 4>;

struct FrameOutput {
    /// Biped128..176. Common cadence reads this frame's up vector144.
    Matrix frame{};
    /// Biped512. Common physical publication copies this to OffBoard64.
    Vector velocity{};

    /// 82D80360 after movement, before82D80548 and cadence82D80720.
    void update(Vector forward_32, Vector frame_up_448, Vector projection_axis_416, Vector position_48);
};
S3_DERIVE_EQ(FrameOutput, frame, velocity)

struct PositionInput {
    Vector previous_origin_112{};
    Vector override_origin_592{};
    bool use_override_711 = false;
    Vector projection_axis_416{};
    std::uint32_t contact_flags_176 = 0;
    Vector contact_origin_0{};
    Vector contact_origin_96{};
    Vector frame_position_176{};
    Vector animation_position_272{};
};

/// 82D80548 updates retained Biped368 using actual ground-job contact/pose data.
void update_position(Vector& position_368, const PositionInput& input);

}  // namespace s3::player::offboard::position_output
