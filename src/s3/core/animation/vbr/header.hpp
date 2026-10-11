// Port of skate-core/src/animation/vbr/header.rs.
// Original TU3 InitPerAnim82E88FA8 and UnPackHeaderBits82E8A780.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "s3/core/animation/slice.hpp"
#include "s3/core/rs.hpp"

namespace s3::animation::vbr::header {

/// pub(super) in Rust.
struct Channel {
    std::size_t dynamic = 0;
    std::size_t constant = 0;
    std::size_t components = 0;
    float min = 0.0f;
    float range = 0.0f;
};

/// `Range<usize>`.
struct ByteRange {
    std::size_t start = 0;
    std::size_t end = 0;
};

/// pub(super) in Rust.
struct Header {
    std::size_t frames = 0;
    std::size_t bones = 0;
    float dct_scale = 0.0f;
    std::vector<Channel> channels;
    std::vector<float> constants;
    std::vector<bool> constant_flags;
    std::vector<std::uint32_t> widths;
    std::vector<ByteRange> blocks;

    static rs::Result<Header, std::string> read(Slice<const std::uint8_t> bytes, std::size_t part,
                                                std::size_t header_relative, std::size_t data_relative);
};

rs::Result<std::size_t, std::string> offset(std::size_t base, std::size_t relative);
rs::Result<Slice<const std::uint8_t>, std::string> take(Slice<const std::uint8_t> bytes, std::size_t at,
                                                        std::size_t count);
rs::Result<std::uint16_t, std::string> u16_at(Slice<const std::uint8_t> bytes, std::size_t at);
rs::Result<std::uint32_t, std::string> u32_at(Slice<const std::uint8_t> bytes, std::size_t at);

}  // namespace s3::animation::vbr::header
