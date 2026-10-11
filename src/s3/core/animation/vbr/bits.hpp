// Port of skate-core/src/animation/vbr/bits.rs.
// TU3 Vector_UnPackFrameBlock82E8A928, expressed as scalar bit readers.
// The native reverse-byte permutation830CEAD0 (initializer82F96DF8) makes
// both streams little endian. Width nibbles come from big-endian header words.
#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "s3/core/animation/slice.hpp"
#include "s3/core/rs.hpp"

namespace s3::animation::vbr::bits {

/// pub(super) in Rust.
rs::Result<std::vector<std::array<float, 8>>, std::string> unpack(Slice<const std::uint8_t> block,
                                                                  Slice<const std::uint32_t> widths);

}  // namespace s3::animation::vbr::bits
