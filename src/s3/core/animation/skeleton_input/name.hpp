// Port of skate-core/src/animation/skeleton_input/name.rs (complete file).
// Native five-word text encoding823C3C78 and six-character chunk82382B50.
// This is an encoded string, not a vault/hash-table key or pointer identity.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "s3/core/animation/output/attributes.hpp"

namespace s3::animation::skeleton_input::name {

using output::attributes::AttributeName;

/// Encode up to30 bytes, stopping at NUL. Native accepts signed bytes and does
/// not validate its alphabet. Preserve its case folding, punctuation behavior,
/// wrapping word arithmetic and truncation; do not substitute UTF-8 casefold.
constexpr AttributeName encode(const std::uint8_t* text, std::size_t len) {
    AttributeName out{};
    std::size_t position = 0;
    while (position < 30 && position < len && text[position] != 0) {
        std::uint32_t weight = 79235168u;  // 38^5, exactly the native first weight.
        const std::size_t word = position / 6;
        int count = 0;
        while (count < 6 && position < len && text[position] != 0) {
            const std::int32_t byte = static_cast<std::int8_t>(text[position]);
            std::int32_t digit = 0;
            if (byte >= 97) {
                digit = byte - 86;
            } else if (byte > 90) {
                digit = byte - 58;
            } else if (byte > 57) {
                digit = byte - 54;
            } else {
                digit = byte - 47;
            }
            out._0[word] = out._0[word] + static_cast<std::uint32_t>(digit) * weight;
            weight /= 38;
            count += 1;
            position += 1;
        }
    }
    return out;
}
/// Convenience overload for byte-string literals (`b"..."` in Rust).
inline AttributeName encode(std::string_view text) {
    return encode(reinterpret_cast<const std::uint8_t*>(text.data()), text.size());
}

}  // namespace s3::animation::skeleton_input::name
