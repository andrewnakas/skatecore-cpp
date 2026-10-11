// Port of skate-core/src/animation/vbr/mod.rs.
#include "s3/core/animation/vbr.hpp"

namespace s3::animation::vbr {

rs::Result<VbrDecoder, std::string> VbrDecoder::new_(Slice<const std::uint8_t> bytes, std::size_t part_offset,
                                                     std::size_t compression_header_relative,
                                                     std::size_t compressed_data_relative) {
    auto read = header::Header::read(bytes, part_offset, compression_header_relative, compressed_data_relative);
    if (read.is_err()) return rs::make_err(read.unwrap_err());
    return VbrDecoder(bytes, std::move(read.unwrap()));
}

rs::Result<std::vector<std::array<std::uint32_t, 10>>, std::string> VbrDecoder::decode_frame(std::size_t frame) {
    if (frame >= header_.frames) {
        return rs::make_err("VBR frame " + std::to_string(frame) + " out of range");
    }
    const std::size_t block_index = frame / 8;
    const header::ByteRange range = header_.blocks[block_index];
    const Slice<const std::uint8_t> block = bytes_.sub(range.start, range.end);
    if (!cached_block_ || cached_block_->first != block_index) {
        auto unpacked = bits::unpack(block.sub(3, block.size()), header_.widths);
        if (unpacked.is_err()) return rs::make_err(unpacked.unwrap_err());
        cached_block_ = std::make_pair(block_index, std::move(unpacked.unwrap()));
    }
    const auto& coefficients = cached_block_->second;
    std::vector<std::array<std::uint32_t, 10>> output(header_.bones, std::array<std::uint32_t, 10>{});
    std::size_t constant = 0;
    std::size_t dynamic = 0;
    std::size_t component_offset = 0;
    for (std::size_t channel_index = 0; channel_index < header_.channels.size(); ++channel_index) {
        const header::Channel& channel = header_.channels[channel_index];
        for (std::size_t bone = 0; bone < output.size(); ++bone) {
            auto& sqt = output[bone];
            const bool is_constant = header_.constant_flags[channel_index * header_.bones + bone];
            for (std::size_t component = 0; component < channel.components; ++component) {
                float value;
                if (is_constant) {
                    value = header_.constants.at(constant);
                    constant += 1;
                } else {
                    value = transform::decode(frame % 8, block[channel_index], header_.dct_scale,
                                              coefficients.at(dynamic), channel.min, channel.range);
                    dynamic += 1;
                }
                sqt[component_offset + component] = rs::to_bits(value);
            }
        }
        component_offset += channel.components;
    }
    return output;
}

}  // namespace s3::animation::vbr
