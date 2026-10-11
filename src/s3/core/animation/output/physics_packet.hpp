// Port of skate-core/src/animation/output/physics_packet.rs.
// Concrete SkaterAnim::GetPhysUpdateData, TU3 82B985E8.
// This owns only fields written by that function, not the full AnimOutPhysIn
// reset, motion-graph merge, attribute dispatch, or physical force consumers.
#pragma once
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "s3/core/animation/commands/batch.hpp"
#include "s3/core/animation/commands/buffers.hpp"
#include "s3/core/animation/output/native.hpp"
#include "s3/core/animation/slice.hpp"

namespace s3::animation::output::physics_packet {

using commands::batch::CompletedSetData;
using commands::buffers::BoneSlice;
using commands::buffers::BufferError;

/// Optional object at SkaterAnim+15104. The original class name is unresolved.
struct AnimationSignal {
    std::uint32_t name_hash = 0;
    std::uint8_t active = 0;
};

/// Fields read or consumed from the concrete native SkaterAnim. Other object
/// fields remain with their owners. The two unknown requests are NOT push flags.
struct SkaterPublicationState {
    /// Native packed flags+15180 bits31,30,29,28 respectively.
    bool orientation_bit31 = false;
    bool mirrored = false;
    bool riding_fakie = false;
    bool weight_on_nose = false;
    /// +15196 and +15188 are distinct enums. Only equality is tested here.
    std::int32_t relative_stance = 0;
    std::int32_t natural_stance = 0;
    /// Native flags bits16 and15, consumed at publication.
    bool request_bit16 = false;
    bool request_bit15 = false;
    /// Native flags bit20 and persistent count+15184.
    bool air_dismount_revert_requested = false;
    std::int32_t air_dismount_revert_frames = 0;
    std::optional<AnimationSignal> signal;
};

struct ActorPoseBuffers {
    /// Actor-owned hierarchy-global matrices, full SkaterAnim+13460.
    BoneSlice<NativeMatrix> hierarchy;
    /// Actor-owned local matrices, full SkaterAnim+13468.
    BoneSlice<NativeMatrix> local;
};

/// Physics-packet-owned arrays are separate from the animation allocations.
/// Count is packet+10380, which native publication reads without rewriting.
/// Unlisted packet fields must be preserved by the surrounding packet owner.
struct PhysicsPosePacket {
    std::uint32_t bone_count = 0;
    std::vector<NativeMatrix> hierarchy;
    std::vector<NativeMatrix> local;
    float timestep = 0.0f;
    std::array<std::uint32_t, 2> foot_surface_ids{};
    std::uint32_t flags = 0;
    bool board_flipped = false;
    bool mirrored = false;
    bool riding_switch = false;
    bool riding_fakie = false;
    bool weight_forwards = false;
    bool regular_stance = false;
    std::int32_t air_dismount_revert_frames = 0;
};

/// Publish after the source worker completes. Before modifying state, this safe
/// adapter rejects invalid/uninitialized ranges; native assumes valid storage.
/// `signal_name` is the current string selected by native global830301E4 (the
/// inspected TU3 image selects "signup"). Supply it from verified startup data.
/// Concrete getters 82B970D8/82B97100/82B97128 are pure field reads here;
/// polymorphic replacement getters are outside this concrete SkaterAnim port.
rs::Result<std::int32_t, BufferError> publish(SkaterPublicationState& state,
                                              const CompletedSetData& completed,
                                              const ActorPoseBuffers& poses, PhysicsPosePacket& packet,
                                              double timestep, Slice<const std::uint8_t> signal_name);

/// Same publication with host-owned evaluated matrices. Completing animation
/// evaluation before this call preserves the source dependency without
/// requiring the original job/cache allocation system in the game.
rs::Result<std::int32_t, BufferError> publish_evaluated(SkaterPublicationState& state,
                                                        Slice<const NativeMatrix> hierarchy,
                                                        Slice<const NativeMatrix> local,
                                                        PhysicsPosePacket& packet, double timestep,
                                                        Slice<const std::uint8_t> signal_name);

/// 8296EA10: NUL-terminated, signed-byte name hashing. This deliberately does
/// not substitute a library hash or unsigned-byte variant.
std::uint32_t signal_hash(Slice<const std::uint8_t> name);
/// Convenience overload for `b"..."` literals.
inline std::uint32_t signal_hash(std::string_view name) {
    return signal_hash(Slice<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(name.data()), name.size()));
}

}  // namespace s3::animation::output::physics_packet
