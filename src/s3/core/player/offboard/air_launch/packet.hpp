// Port of skate-core/src/player/offboard/air_launch/packet.rs (whole file).
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/rs.hpp"

namespace s3::player::offboard::air_launch::packet {

using Vector = std::array<float, 4>;

/// Native118-byte logical packet. W lanes and the two trailing bytes are real
/// fields; this host struct makes no promise about the C ABI or native padding.
struct Packet {
    Vector velocity_0{};
    Vector secondary_velocity_16{};
    Vector position_32{};
    Vector up_48{};
    Vector forward_64{};
    Vector board_position_80{};
    float scalar_96 = 0.0f;
    float scalar_100 = 0.0f;
    float scalar_104 = 0.0f;
    std::uint32_t kind_108 = 0;
    std::uint32_t kind_112 = 0;
    bool has_board_position_116 = false;
    bool flag_117 = false;

    ///82D2DB98 does not write104; the caller supplies its retained value.
    /// The launch producer subsequently overwrites104 in every mode.
    static Packet initialized(float retained_104) {
        Packet p;
        p.up_48 = {0.0f, 1.0f, 0.0f, 0.0f};
        p.scalar_104 = retained_104;
        p.kind_108 = 1;
        p.kind_112 = 0;
        return p;
    }
};
S3_DERIVE_EQ(Packet, velocity_0, secondary_velocity_16, position_32, up_48, forward_64, board_position_80, scalar_96,
             scalar_100, scalar_104, kind_108, kind_112, has_board_position_116, flag_117)

}  // namespace s3::player::offboard::air_launch::packet
