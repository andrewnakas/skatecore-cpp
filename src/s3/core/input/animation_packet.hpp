// Port of skate-core/src/input/animation_packet.rs.
// Complete TU3 82DB5BE0: AnimOutPhysIn -> ProcessedPhysIn publication.
// Called by physical input stage 82DB4048 before board/attribute processing.
// Inputs are already produced by animation; this is not a gamepad mapping.
#pragma once
#include <array>
#include <cstdint>

namespace s3::input::animation_packet {

/// Source packet fields. Byte flags use their low bit, including noncanonical
/// byte values. Matrix/vector words are copied without numerical conversion.
struct AnimationPacketFields {
    /// Packet +10368 -> processed +2468 bit 20.
    std::uint8_t stance_byte = 0;
    /// Packet +10384 / +10388.
    float timestep = 0.0f;
    float scalar_10388 = 0.0f;
    /// Packet +10375, +10496, +10784 -> processed +2468 bits 3, 2, 1.
    std::array<std::uint8_t, 3> flags_10375_10496_10784{};
    std::array<std::uint32_t, 4> vector_10480{};
    std::array<std::array<std::uint32_t, 4>, 4> matrix_10704{};
    std::uint8_t byte_10768 = 0;
    /// Packet +10788 / +10792.
    float truck_tightness = 0.0f;
    float scalar_10792 = 0.0f;
    /// Packet +10371 -> processed +2476 bit 20.
    std::uint8_t flag_10371 = 0;
};

/// All fields written by 82DB5BE0. Seed flag words from the preceding native
/// reset/publication stages: this helper preserves their unrelated bits.
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

/// Publish the complete function's writes into distinct source/destination
/// records. The source packet must remain valid through physical processing.
void publish(const AnimationPacketFields& source, ProcessedPacketFields& output);

}  // namespace s3::input::animation_packet
