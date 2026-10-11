// Port of skate-core/src/input/xbox.rs.
// Complete TU3 Xbox pad conversion 8296D5F8, after successful platform polling.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/rs.hpp"

namespace s3::input::xbox {

/// Fields of the native Xbox state passed by value to 8296D5F8.
struct XboxState {
    std::uint16_t buttons = 0;
    std::array<std::uint8_t, 2> triggers{};
    std::array<std::int16_t, 2> left{};
    std::array<std::int16_t, 2> right{};
};
S3_DERIVE_EQ(XboxState, buttons, triggers, left, right)

/// Returns all 24 native button values. `device_byte_13` is explicit because
/// its producer/meaning is not inferred from the conversion's four clear stores.
std::array<float, 24> convert(const XboxState& state, std::uint8_t device_byte_13);

}  // namespace s3::input::xbox
