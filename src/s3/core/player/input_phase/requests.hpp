// Port of skate-core/src/player/input_phase/requests.rs (whole file).
#pragma once
#include <cstdint>

#include "s3/core/player/input_phase/raw.hpp"
#include "s3/core/rs.hpp"

namespace s3::player::input_phase::requests {

using types::RawVector;

struct GroundHistoryRequest {
    RawVector previous_position{};
    RawVector current_position{};
    RawVector previous_filtered_delta{};
    /// Processed +2604. The native block also consumes constants at
    /// `0x82072818` and `0x8231A844`; the service owns those exact values.
    std::uint32_t timestep_bits = 0;
};
S3_DERIVE_EQ(GroundHistoryRequest, previous_position, current_position, previous_filtered_delta, timestep_bits)

struct GroundHistoryResult {
    RawVector delta{};
    RawVector filtered_delta{};
};
S3_DERIVE_EQ(GroundHistoryResult, delta, filtered_delta)

struct PrepareJumpRequest {
    RawVector skateboard_vector{};
    RawVector previous_velocity{};
};
S3_DERIVE_EQ(PrepareJumpRequest, skateboard_vector, previous_velocity)

}  // namespace s3::player::input_phase::requests
