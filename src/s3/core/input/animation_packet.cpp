// Port of skate-core/src/input/animation_packet.rs.
// Complete TU3 82DB5BE0: AnimOutPhysIn -> ProcessedPhysIn publication.
#include "s3/core/input/animation_packet.hpp"

namespace s3::input::animation_packet {

namespace {
void replace_bit(std::uint32_t& word, std::uint32_t bit, std::uint8_t byte) {
    word = (word & ~(1u << bit)) | (std::uint32_t(byte & 1u) << bit);
}
}  // namespace

void publish(const AnimationPacketFields& source, ProcessedPacketFields& output) {
    replace_bit(output.flags_2468, 20, source.stance_byte);
    output.timestep = source.timestep;
    output.scalar_2668 = source.scalar_10388;
    replace_bit(output.flags_2468, 3, source.flags_10375_10496_10784[0]);
    replace_bit(output.flags_2468, 2, source.flags_10375_10496_10784[1]);
    output.vector_1520 = source.vector_10480;
    replace_bit(output.flags_2468, 1, source.flags_10375_10496_10784[2]);
    output.matrix_1536 = source.matrix_10704;
    output.byte_1600 = source.byte_10768;
    output.truck_tightness = source.truck_tightness;
    output.scalar_2764 = source.scalar_10792;
    replace_bit(output.flags_2476, 20, source.flag_10371);
}

}  // namespace s3::input::animation_packet
