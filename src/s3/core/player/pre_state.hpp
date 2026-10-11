// Port of skate-core/src/player/pre_state.rs (whole file).
// Complete TU3 `PhysicalPlayerHiLOD::PreState` (`0x82DB6050`).
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/rs.hpp"

namespace s3::player::pre_state {

/// The 72-byte stack packet filled through current-state vtable slot `+24`.
/// Only the vector at byte `+48` is consumed by the player wrapper, but the
/// entire packet remains part of the state callback contract.
struct PreStatePacket {
    std::array<std::uint32_t, 18> words{};

    static constexpr PreStatePacket ZERO() { return PreStatePacket{}; }
    constexpr std::array<std::uint32_t, 4> vector_48() const {
        return {words[12], words[13], words[14], words[15]};
    }
};
S3_DERIVE_EQ(PreStatePacket, words)

struct PreStatePlayerFields {
    /// PhysicalPlayer+1312.
    std::uint32_t frame_counter_1312 = 0;
    /// Whether PhysicalPlayer+1840 owns the optional pre-state component.
    bool component_1840_present = false;
};
S3_DERIVE_EQ(PreStatePlayerFields, frame_counter_1312, component_1840_present)

struct PreStateSkeletonFields {
    /// Skeleton+16112.
    std::array<std::uint32_t, 4> predicted_position_16112{};
    /// Skeleton+16416.
    bool predicted_position_set_16416 = false;
    /// Skeleton+16432 -> nested object +3184.
    bool nested_flag_3184 = false;
};
S3_DERIVE_EQ(PreStateSkeletonFields, predicted_position_16112, predicted_position_set_16416, nested_flag_3184)

/// Every call made by the wrapper, in native order. There is deliberately no
/// default implementation: state and component behavior must be supplied by
/// their recovered modules.
struct PreStateServices {
    virtual ~PreStateServices() = default;
    /// Current-state vtable slot `+24`.
    virtual void fill_packet_vtable_24(PreStatePacket& packet) = 0;
    /// `0x82D74270`, reached only when Player+1840 is present.
    virtual void update_component_1840_82d74270() = 0;
    /// Current-state vtable slot `+4`.
    virtual void update_before_state_vtable_4() = 0;
};

inline void run_pre_state(PreStatePlayerFields& player, PreStateSkeletonFields& skeleton,
                          PreStateServices& services) {
    player.frame_counter_1312 = player.frame_counter_1312 + 1u;
    PreStatePacket packet = PreStatePacket::ZERO();
    services.fill_packet_vtable_24(packet);
    skeleton.predicted_position_16112 = packet.vector_48();
    skeleton.predicted_position_set_16416 = true;
    skeleton.nested_flag_3184 = false;
    if (player.component_1840_present) {
        services.update_component_1840_82d74270();
    }
    services.update_before_state_vtable_4();
}

}  // namespace s3::player::pre_state
