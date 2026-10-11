// Port of skate-core/src/animation/vbr/transform.rs.
// TU3 Init82E891D8, IDCT table82E88C58, DecompressFrameBlock82E89770.
// All coefficient words below were read from the original TU3 mapped image.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace s3::animation::vbr::transform {

/// pub(super) in Rust.
float decode(std::size_t frame, std::uint8_t quantization_index, float scale, std::array<float, 8> coefficients,
             float channel_min, float channel_range);

}  // namespace s3::animation::vbr::transform
