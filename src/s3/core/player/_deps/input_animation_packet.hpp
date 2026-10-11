// TEMP DEP (stage P2): replace with stage P1 header `s3/core/input/animation_packet.hpp`.
// Faithful port of skate-core/src/input/animation_packet.rs (82DB5BE0), used only
// while the stage-P1 header is missing.
#pragma once
#if __has_include("s3/core/input/animation_packet.hpp")
#include "s3/core/input/animation_packet.hpp"
#else
#include <array>
#include <cstdint>

#include "s3/core/rs.hpp"

namespace s3::input::animation_packet {

struct AnimationPacketFields {
    std::uint8_t stance_byte = 0;
    float timestep = 0.0f;
    float scalar_10388 = 0.0f;
    std::array<std::uint8_t, 3> flags_10375_10496_10784{};
    std::array<std::uint32_t, 4> vector_10480{};
    std::array<std::array<std::uint32_t, 4>, 4> matrix_10704{};
    std::uint8_t byte_10768 = 0;
    float truck_tightness = 0.0f;
    float scalar_10792 = 0.0f;
    std::uint8_t flag_10371 = 0;
};

struct ProcessedPacketFields {
    std::uint32_t flags_2468 = 0;
    std::uint32_t flags_2476 = 0;
    float timestep = 0.0f;
    float scalar_2668 = 0.0f;
    std::array<std::uint32_t, 4> vector_1520{};
    std::array<std::array<std::uint32_t, 4>, 4> matrix_1536{};
    std::uint8_t byte_1600 = 0;
    float truck_tightness = 0.0f;
    float scalar_2764 = 0.0f;
};

namespace detail {
inline void replace_bit(std::uint32_t& word, std::uint32_t bit, std::uint8_t byte) {
    word = (word & ~(1u << bit)) | (std::uint32_t(byte & 1u) << bit);
}
}  // namespace detail

inline void publish(const AnimationPacketFields& source, ProcessedPacketFields& output) {
    detail::replace_bit(output.flags_2468, 20, source.stance_byte);
    output.timestep = source.timestep;
    output.scalar_2668 = source.scalar_10388;
    detail::replace_bit(output.flags_2468, 3, source.flags_10375_10496_10784[0]);
    detail::replace_bit(output.flags_2468, 2, source.flags_10375_10496_10784[1]);
    output.vector_1520 = source.vector_10480;
    detail::replace_bit(output.flags_2468, 1, source.flags_10375_10496_10784[2]);
    output.matrix_1536 = source.matrix_10704;
    output.byte_1600 = source.byte_10768;
    output.truck_tightness = source.truck_tightness;
    output.scalar_2764 = source.scalar_10792;
    detail::replace_bit(output.flags_2476, 20, source.flag_10371);
}

}  // namespace s3::input::animation_packet
#endif
