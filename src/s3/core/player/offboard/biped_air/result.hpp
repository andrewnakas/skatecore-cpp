// Port of skate-core/src/player/offboard/biped_air/result.rs (whole file).
// TU3 82D2DAB8 result storage and 82D30300 OffBoard publication.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/rs.hpp"

namespace s3::player::offboard::biped_air::recovered::result {

using Vector = std::array<float, 4>;

struct TrajectoryResult {
    Vector position_272{};
    Vector velocity_288{};
    Vector normal_304{};
    Vector contact_velocity_320{};
    Vector contact_position_336{};
    Vector adjustment_352{};
    Vector apex_368{};
    float time_remaining_384 = 0.0f;
    float duration_388 = 0.0f;
    float scalar_392 = 0.0f;
    float apex_time_396 = 0.0f;
    std::int32_t frame_400 = 0;
    bool valid_404 = false;
    std::uint32_t word_408 = 0;

    ///82D2DAB8 retains result+80/+96/+124. Do not zero the whole packet.
    void reset() {
        position_272 = Vector{};
        velocity_288 = Vector{};
        normal_304 = Vector{};
        contact_velocity_320 = Vector{};
        contact_position_336 = Vector{};
        time_remaining_384 = 0.0f;
        duration_388 = 0.0f;
        scalar_392 = 0.0f;
        frame_400 = 0;
        valid_404 = false;
        word_408 = 0;
    }
};
S3_DERIVE_EQ(TrajectoryResult, position_272, velocity_288, normal_304, contact_velocity_320, contact_position_336,
             adjustment_352, apex_368, time_remaining_384, duration_388, scalar_392, apex_time_396, frame_400,
             valid_404, word_408)

///Every offset is relative to PhysOut's OffBoard child, never Air.
struct OffBoardOutput {
    float scalar_32 = 0.0f;
    Vector vector_64{};
    float scalar_92 = 0.0f;
    Vector vector_96{};
    std::uint32_t word_144 = 0;
    float scalar_148 = 0.0f;
    float scalar_152 = 0.0f;
    float scalar_156 = 0.0f;
    Vector vector_160{};
    Vector vector_176{};
    Vector vector_192{};
    Vector vector_208{};
    Vector vector_224{};
    Vector vector_240{};
    bool flag_320 = false;
    bool flag_328 = false;
    bool flag_331 = false;
};
S3_DERIVE_EQ(OffBoardOutput, scalar_32, vector_64, scalar_92, vector_96, word_144, scalar_148, scalar_152,
             scalar_156, vector_160, vector_176, vector_192, vector_208, vector_224, vector_240, flag_320, flag_328,
             flag_331)

}  // namespace s3::player::offboard::biped_air::recovered::result
