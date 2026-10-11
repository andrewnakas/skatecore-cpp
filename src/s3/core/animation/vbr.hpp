// Port of skate-core/src/animation/vbr/mod.rs.
// Direct stock VBR SQT decoding from original Skate 3 TU3.
// Source:82E88FA8/82E8A780(header),82E8A928(bits),82E89770(transform),
// 82E89A60/82E88BA0(output), and82D20298(SQT channel descriptors).
// Host allocation/cache ownership is independent. PC floating-point arithmetic
// is not claimed to emulate the Xenon's numerical instructions bit for bit.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "s3/core/animation/slice.hpp"
#include "s3/core/animation/vbr/bits.hpp"
#include "s3/core/animation/vbr/header.hpp"
#include "s3/core/animation/vbr/transform.hpp"

namespace s3::animation::vbr {

/// Borrows `bytes` (Rust lifetime 'a): the caller keeps the buffer alive.
class VbrDecoder {
public:
    static rs::Result<VbrDecoder, std::string> new_(Slice<const std::uint8_t> bytes, std::size_t part_offset,
                                                    std::size_t compression_header_relative,
                                                    std::size_t compressed_data_relative);
    std::size_t frame_count() const { return header_.frames; }
    std::size_t channel_count() const { return header_.bones; }

    ///Return each authored bone's scaleXYZ, quaternionXYZW, translationXYZ bits.
    ///No quaternion normalization, retargeting, hierarchy changes or weight edits.
    rs::Result<std::vector<std::array<std::uint32_t, 10>>, std::string> decode_frame(std::size_t frame);

private:
    VbrDecoder(Slice<const std::uint8_t> bytes, header::Header header)
        : bytes_(bytes), header_(std::move(header)) {}
    Slice<const std::uint8_t> bytes_;
    header::Header header_;
    std::optional<std::pair<std::size_t, std::vector<std::array<float, 8>>>> cached_block_;
};

}  // namespace s3::animation::vbr
