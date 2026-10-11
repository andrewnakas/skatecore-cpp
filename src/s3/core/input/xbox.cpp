// Port of skate-core/src/input/xbox.rs.
// Complete TU3 Xbox pad conversion 8296D5F8, after successful platform polling.
#include "s3/core/input/xbox.hpp"

#include "s3/core/input/controller.hpp"

namespace s3::input::xbox {

using controller::magnitude;
using rs::from_bits;

namespace {

float positive(float value) {
    // fsel(-value, +0, value), including the native signed-zero selection.
    if (-value >= 0.0f) {
        return 0.0f;
    }
    return value;
}

std::array<float, 4> condition(std::array<std::int16_t, 2> stick) {
    float x = float(stick[0]) * from_bits(0x38000000u);
    float y = float(stick[1]) * from_bits(0x38000000u);
    const float squared = rs::mul_add(y, y, x * x);
    const float length = magnitude(squared);
    float factor;
    if (length < from_bits(0x3a83126fu)) {
        factor = 0.0f;
    } else {
        const float scaled = (length - 0.25f) * from_bits(0x3fb6db6eu);
        const float bounded = scaled < 0.0f ? 0.0f : (scaled > 1.0f ? 1.0f : scaled);
        factor = bounded / length;
    }
    x = factor * x;
    y = factor * y;
    return {positive(x), positive(-x), positive(y), positive(-y)};
}

}  // namespace

std::array<float, 24> convert(const XboxState& state, std::uint8_t device_byte_13) {
    std::array<float, 24> result{};
    for (unsigned bit = 0; bit < 10; ++bit) {
        result[bit] = (state.buttons & (1u << bit)) != 0 ? 1.0f : 0.0f;
    }
    result[10] = float(state.triggers[0]) * from_bits(0x3b808081u);
    result[11] = float(state.triggers[1]) * from_bits(0x3b808081u);
    for (unsigned bit = 12; bit <= 15; ++bit) {
        result[bit] = (state.buttons & (1u << bit)) != 0 ? 1.0f : 0.0f;
    }
    const auto left = condition(state.left);
    const auto right = condition(state.right);
    for (std::size_t i = 0; i < 4; ++i) {
        result[16 + i] = left[i];
        result[20 + i] = right[i];
    }
    if (device_byte_13 != 0) {
        for (std::size_t slot : {10u, 11u, 21u, 22u}) {
            result[slot] = 0.0f;
        }
    }
    return result;
}

}  // namespace s3::input::xbox
