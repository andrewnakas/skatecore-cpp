// Port of skate-core/src/animation/output/motion_graph_packet.rs.
// Concrete SkaterMotionGraph::GetPhysUpdateData82595FA8 and copy82596008.
// Actor construction82590DC0 ->8258F488 installs base vtable823006D0;
// slot36 resolves82595FA8. Unlike the reviewed Skate2 counterpart, this
// Skate3 body does not reset trick, gesture or wipeout source state afterward.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/rs.hpp"

namespace s3::animation::output::motion_graph_packet {

/// Native name storage at the data boundary; no addresses or host pointers.
/// Exact text/hash member interpretation is a separate string-format contract.
/// Rust tuple struct `TrickIdentifier(pub [u32; 6])`.
struct TrickIdentifier {
    std::array<std::uint32_t, 6> _0{};
};
S3_DERIVE_EQ(TrickIdentifier, _0)

struct TrickData {
    std::array<TrickIdentifier, 2> identifiers{};
    /// Native trick record+48. Its producer/enum meaning is not recovered here.
    std::uint32_t word48 = 0;
    /// Native record+52/+56; copied by lfs/stfs.
    std::array<float, 2> scalars52_56{};
};
S3_DERIVE_EQ(TrickData, identifiers, word48, scalars52_56)

struct GestureData {
    std::uint32_t gesture = 0;
    /// The native body copies the complete second word, including unused bytes.
    std::array<std::uint8_t, 4> flag_bytes{};
};
S3_DERIVE_EQ(GestureData, gesture, flag_bytes)

struct MotionGraphPacket {
    /// Source MG+5924 -> physics packet+10392,60 bytes.
    TrickData trick;
    /// MG+5984/+5988 -> packet+10452/+10456.
    GestureData gesture;
    /// MG+5992/+5996 -> packet+10460/+10464; complete words are copied.
    std::array<float, 2> wipeout_gesture{};
};
S3_DERIVE_EQ(MotionGraphPacket, trick, gesture, wipeout_gesture)

/// Complete concrete publication. Timestep is unused by82595FA8; no source
/// request is consumed, and repeated publication copies the same state.
inline void publish(const MotionGraphPacket& source, MotionGraphPacket& destination) {
    destination.trick = source.trick;
    destination.gesture.gesture = source.gesture.gesture;
    destination.gesture.flag_bytes = source.gesture.flag_bytes;
    destination.wipeout_gesture[0] = source.wipeout_gesture[0];
    destination.wipeout_gesture[1] = source.wipeout_gesture[1];
}

}  // namespace s3::animation::output::motion_graph_packet
