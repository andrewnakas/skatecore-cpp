// Port of skate-core/src/animation/output/physics_packet.rs.
#include "s3/core/animation/output/physics_packet.hpp"

namespace s3::animation::output::physics_packet {

namespace {
void replace_flag(std::uint32_t& flags, std::uint32_t bit, bool value) {
    flags = (flags & ~(1u << bit)) | (static_cast<std::uint32_t>(value) << bit);
}
}  // namespace

rs::Result<std::int32_t, BufferError> publish(SkaterPublicationState& state,
                                              const CompletedSetData& completed,
                                              const ActorPoseBuffers& poses, PhysicsPosePacket& packet,
                                              double timestep, Slice<const std::uint8_t> signal_name) {
    const std::size_t count = static_cast<std::size_t>(packet.bone_count);
    if (packet.hierarchy.size() < count || packet.local.size() < count) {
        return rs::make_err(BufferError(BufferError::RangeOutsideAllocation{}));
    }
    auto hierarchy = completed.buffers().matrices.read(poses.hierarchy, count);
    if (hierarchy.is_err()) return rs::make_err(hierarchy.unwrap_err());
    auto local = completed.buffers().matrices.read(poses.local, count);
    if (local.is_err()) return rs::make_err(local.unwrap_err());
    return publish_evaluated(state, hierarchy.unwrap(), local.unwrap(), packet, timestep, signal_name);
}

rs::Result<std::int32_t, BufferError> publish_evaluated(SkaterPublicationState& state,
                                                        Slice<const NativeMatrix> hierarchy,
                                                        Slice<const NativeMatrix> local,
                                                        PhysicsPosePacket& packet, double timestep,
                                                        Slice<const std::uint8_t> signal_name) {
    const std::size_t count = static_cast<std::size_t>(packet.bone_count);
    if (hierarchy.size() < count || local.size() < count || packet.hierarchy.size() < count ||
        packet.local.size() < count) {
        return rs::make_err(BufferError(BufferError::RangeOutsideAllocation{}));
    }

    packet.timestep = static_cast<float>(timestep);
    packet.foot_surface_ids = {0, 0};
    replace_flag(packet.flags, 27, state.request_bit16);
    state.request_bit16 = false;
    replace_flag(packet.flags, 23, state.request_bit15);
    state.request_bit15 = false;
    packet.flags &= ~((1u << 25) | (1u << 26));
    if (state.signal) {
        state.signal->name_hash = signal_hash(signal_name);
        state.signal->active = 0;
    }
    for (std::size_t i = 0; i < count; ++i) packet.hierarchy[i] = hierarchy[i];
    for (std::size_t i = 0; i < count; ++i) packet.local[i] = local[i];
    packet.board_flipped = state.orientation_bit31 != state.mirrored;
    packet.mirrored = state.mirrored;
    packet.riding_switch = state.relative_stance == 1;
    packet.riding_fakie = state.riding_fakie;
    packet.weight_forwards = state.riding_fakie ? !state.weight_on_nose : state.weight_on_nose;
    packet.regular_stance = state.natural_stance == 0;
    replace_flag(packet.flags, 28, state.air_dismount_revert_requested);
    state.air_dismount_revert_requested = false;
    packet.air_dismount_revert_frames = state.air_dismount_revert_frames;
    return state.air_dismount_revert_frames;
}

std::uint32_t signal_hash(Slice<const std::uint8_t> name) {
    std::uint32_t hash = 0;
    for (std::uint8_t byte : name) {
        if (byte == 0) break;
        hash = (hash << 4) + static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(byte)));
        const std::uint32_t high = hash & 0xf0000000u;
        if (high != 0) {
            hash ^= (high >> 23) ^ high;
        }
    }
    return hash;
}

}  // namespace s3::animation::output::physics_packet
