// Port of skate-core/src/animation/vbr/bits.rs.
#include "s3/core/animation/vbr/bits.hpp"

namespace s3::animation::vbr::bits {

namespace {
struct Bits {
    Slice<const std::uint8_t> bytes;
    std::size_t position = 0;

    rs::Result<std::uint32_t, std::string> read(std::uint32_t count) {
        std::uint32_t value = 0;
        for (std::uint32_t bit = 0; bit < count; ++bit) {
            const std::uint8_t* byte = bytes.get(position / 8);
            if (!byte) return rs::make_err(std::string("VBR coefficient bit stream truncated"));
            value |= static_cast<std::uint32_t>((*byte >> (position % 8)) & 1u) << bit;
            position += 1;
        }
        return value;
    }
};
}  // namespace

rs::Result<std::vector<std::array<float, 8>>, std::string> unpack(Slice<const std::uint8_t> block,
                                                                  Slice<const std::uint32_t> widths) {
    if (widths.is_empty()) return std::vector<std::array<float, 8>>{};
    if (block.is_empty()) return rs::make_err(std::string("VBR block has no selector length"));
    const std::size_t prefix = block[0];
    // `block.get(1..)` always succeeds for a nonempty block.
    const Slice<const std::uint8_t> selectors = block.sub(1, block.size());
    const std::size_t magnitudes_start = 1 + (prefix + 7) / 8;
    if (magnitudes_start > block.size()) {
        return rs::make_err(std::string("VBR selector length exceeds block"));
    }
    const Slice<const std::uint8_t> magnitudes = block.sub(magnitudes_start, block.size());
    Bits selected{selectors, 0};
    Bits packed{magnitudes, 0};
    std::vector<std::array<float, 8>> output;
    output.reserve(widths.size());
    for (std::uint32_t descriptor : widths) {
        std::array<float, 8> coefficients{};
        for (std::size_t i = 0; i < 8; ++i) {
            const std::uint32_t width = (descriptor >> (4 * i)) & 15u;
            // A zero width consumes neither a selector nor packed data.
            if (width == 0) continue;
            auto sel = selected.read(1);
            if (sel.is_err()) return rs::make_err(sel.unwrap_err());
            if (sel.unwrap() == 0) continue;
            auto sign = packed.read(1);
            if (sign.is_err()) return rs::make_err(sign.unwrap_err());
            const bool positive = sign.unwrap() != 0;
            auto mag = packed.read(width);
            if (mag.is_err()) return rs::make_err(mag.unwrap_err());
            const float magnitude = static_cast<float>(mag.unwrap());
            // The original complements the sign bit, then shifts it to
            // bit31 and ORs it into the converted unsigned magnitude.
            // Preserve negative zero; this is not two's complement.
            coefficients[i] = rs::from_bits(rs::to_bits(magnitude) | (positive ? 0u : 0x80000000u));
        }
        output.push_back(coefficients);
    }
    return output;
}

}  // namespace s3::animation::vbr::bits
