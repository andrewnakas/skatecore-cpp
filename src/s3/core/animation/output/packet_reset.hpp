// Port of skate-core/src/animation/output/packet_reset.rs.
// AnimOutPhysIn::Reset, TU3 82590028.
// Reset is selective: it preserves storage, count, intents, attributes and all
// fields not listed below. Actor::SetUpPhysics owns the later list replacement.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/animation/commands/buffers.hpp"
#include "s3/core/animation/output/native.hpp"
#include "s3/core/animation/output/physics_packet.hpp"

namespace s3::animation::output::packet_reset {

using commands::buffers::BufferError;
using physics_packet::PhysicsPosePacket;

/// Additional fields written by Reset, kept separate from the existing pose
/// publication view. Unknown flags retain their verified producer/offset names.
struct AdditionalResetFields {
    /// 10388; 10912/10916.
    float compression = 0.0f;
    std::array<float, 2> foot_ik_influence{};
    /// 10369. Position itself is preserved by Reset.
    bool next_step_position_valid = false;
    /// 10375/10376; overwritten from these Actor bits during SetUpPhysics.
    bool actor_flag_1904_bit23 = false;
    bool actor_flag_1908_bit2 = false;
    /// 10496,10688,10689: active markers only; payloads are not cleared here.
    bool external_impulse_active = false;
    bool external_physics_input_active = false;
    bool externally_controlled = false;
    /// 10786/10787/10796.
    bool prevent_manual_respawn = false;
    std::uint8_t ignore_respawn_reset_button = 0;
    bool force_braking = false;
    /// 10788/10792. Named source fields, same target scalar reset/caller roles.
    float truck_tightness = 0.0f;
    float wheel_hardness = 0.0f;
    /// Six complete native vectors at10816,10832,10848,10864,10880,10896.
    /// Individual target semantics remain unassigned; all fourth lanes reset.
    std::array<std::array<float, 4>, 6> auxiliary_vectors{};
    /// 10928, later overwritten by the Actor's selected physics settings.
    std::uint32_t requested_physics_mode = 0;
};

/// Native packed basis, NOT a conventional homogeneous 4x4 identity.
/// Three static vectors at82139A10/20/30 plus an all-zero vector produced
/// inside Reset. All four fourth lanes, including translation W, are zero.
inline constexpr NativeMatrix RESET_POSE = {{
    {1.0f, 0.0f, 0.0f, 0.0f},
    {0.0f, 1.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 1.0f, 0.0f},
    {0.0f, 0.0f, 0.0f, 0.0f},
}};

/// Complete field coverage for82590028 on valid packet allocations. Host-side
/// extent checks precede mutation; native assumes allocations match the count.
/// Existing vector lengths/capacities and bones past packet.bone_count remain.
inline rs::Result<rs::Unit, BufferError> reset(PhysicsPosePacket& packet, AdditionalResetFields& fields) {
    // Both native loop comparisons are signed. A high-bit count skips the
    // matrix loop while scalar/flag resets still run.
    const std::int32_t signed_count = static_cast<std::int32_t>(packet.bone_count);
    const std::size_t count = signed_count < 0 ? 0 : static_cast<std::size_t>(signed_count);
    if (packet.hierarchy.size() < count || packet.local.size() < count) {
        return rs::make_err(BufferError(BufferError::RangeOutsideAllocation{}));
    }
    packet.timestep = rs::from_bits(0x3c888889u);
    fields.external_impulse_active = false;
    fields.compression = 0.5f;
    fields.foot_ik_influence[0] = 0.0f;
    fields.external_physics_input_active = false;
    fields.foot_ik_influence[1] = 0.0f;
    fields.externally_controlled = false;
    fields.truck_tightness = 0.0f;
    packet.mirrored = false;
    packet.riding_switch = false;
    packet.riding_fakie = false;
    packet.weight_forwards = false;
    packet.regular_stance = false;
    fields.prevent_manual_respawn = false;
    fields.ignore_respawn_reset_button = 0;
    fields.force_braking = false;
    packet.air_dismount_revert_frames = 0;
    packet.board_flipped = false;
    fields.next_step_position_valid = false;
    fields.actor_flag_1904_bit23 = false;
    packet.flags &= 0x013fffffu;
    fields.actor_flag_1908_bit2 = false;
    packet.foot_surface_ids[0] = 0;
    fields.wheel_hardness = 0.0f;
    packet.foot_surface_ids[1] = 0;
    fields.auxiliary_vectors[0] = {0.0f, 0.0f, 0.0f, 0.0f};
    fields.requested_physics_mode = 1;
    for (std::size_t i = 1; i < fields.auxiliary_vectors.size(); ++i) {
        fields.auxiliary_vectors[i] = {0.0f, 0.0f, 0.0f, 0.0f};
    }
    for (std::size_t bone = 0; bone < count; ++bone) {
        // Native completes local first, then hierarchy, for each bone.
        packet.local[bone] = RESET_POSE;
        packet.hierarchy[bone] = RESET_POSE;
    }
    return rs::Unit{};
}

}  // namespace s3::animation::output::packet_reset
