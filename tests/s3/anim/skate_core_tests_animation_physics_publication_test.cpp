// Port of skate-core/tests/animation_physics_publication.rs (crate integration test).
#include <vector>

#include "harness.hpp"
#include "s3/core/animation/commands.hpp"
#include "s3/core/animation/output/physics_packet.hpp"

using namespace s3::animation::output::physics_packet;
namespace physics_packet = s3::animation::output::physics_packet;
using s3::animation::commands::batch::CompletedSetData;
using s3::animation::commands::batch::SetDataQueue;
using s3::animation::commands::buffers::BufferError;
using s3::animation::commands::buffers::PoseBuffers;
using s3::animation::commands::set_data::CopyCommand;
using s3::animation::commands::set_data::InternalUpdate;
using s3::animation::commands::set_data::SetDataCommand;
using s3::animation::output::NativeMatrix;

namespace {
NativeMatrix matrix(float v) { return NativeMatrix{{{v, v, v, v}, {v, v, v, v}, {v, v, v, v}, {v, v, v, v}}}; }
SkaterPublicationState state() {
    SkaterPublicationState s;
    s.orientation_bit31 = false;
    s.mirrored = false;
    s.riding_fakie = false;
    s.weight_on_nose = false;
    s.relative_stance = 0;
    s.natural_stance = 0;
    s.request_bit16 = true;
    s.request_bit15 = true;
    s.air_dismount_revert_requested = true;
    s.air_dismount_revert_frames = 17;
    s.signal = AnimationSignal{0, 255};
    return s;
}
PhysicsPosePacket packet() {
    PhysicsPosePacket p;
    p.bone_count = 2;
    p.hierarchy = std::vector<NativeMatrix>(3, matrix(-1.0f));
    p.local = std::vector<NativeMatrix>(3, matrix(-2.0f));
    p.timestep = 0.0f;
    p.foot_surface_ids = {35, 46};
    p.flags = UINT32_MAX;
    return p;
}
CompletedSetData complete(SetDataQueue q) {
    auto r = q.submit().complete();
    if (r.is_err()) S3_PANIC("set data failed");
    return std::move(r.unwrap());
}
const std::vector<std::optional<s3::animation::commands::batch::JobId>> NO_DEPS{};
s3::animation::Slice<const std::uint8_t> bytes(std::string_view text) {
    return s3::animation::Slice<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(text.data()), text.size());
}
}  // namespace

S3_TEST(publication_copies_separate_arrays_consumes_latches_and_preserves_unrelated_flags) {
    PoseBuffers buffers{};
    const auto hierarchy = buffers.matrices.initialized({matrix(1.0f), matrix(2.0f)});
    const auto local = buffers.matrices.initialized({matrix(3.0f), matrix(4.0f)});
    const CompletedSetData completed = complete(SetDataQueue::new_(std::move(buffers), 1));
    const ActorPoseBuffers poses{hierarchy.at(0), local.at(0)};
    SkaterPublicationState s = state();
    PhysicsPosePacket p = packet();
    {
        const auto r = physics_packet::publish(s, completed, poses, p, 1.0 / 60.0, bytes("signup"));
        CHECK(r.is_ok() && r.unwrap() == 17);
    }
    CHECK((p.hierarchy == std::vector<NativeMatrix>{matrix(1.0f), matrix(2.0f), matrix(-1.0f)}));
    CHECK((p.local == std::vector<NativeMatrix>{matrix(3.0f), matrix(4.0f), matrix(-2.0f)}));
    CHECK_EQ(p.flags, UINT32_MAX & ~((1u << 25) | (1u << 26)));
    CHECK((p.foot_surface_ids == std::array<std::uint32_t, 2>{0, 0}));
    CHECK_EQ(s3::rs::to_bits(p.timestep), s3::rs::to_bits(1.0f / 60.0f));
    CHECK(!s.request_bit16 && !s.request_bit15 && !s.air_dismount_revert_requested);
    CHECK_EQ(s.signal->active, std::uint8_t(0));
    // ASCII shift/add evaluated independently: no overflow fold for six bytes.
    CHECK_EQ(s.signal->name_hash, 127919552u);
    p.hierarchy[0] = matrix(100.0f);
    CHECK((completed.buffers().matrices.read(hierarchy.at(0), 1).unwrap() == std::vector<NativeMatrix>{matrix(1.0f)}));
    physics_packet::publish(s, completed, poses, p, 0.25, bytes("signup")).unwrap();
    CHECK_EQ(p.flags & ((1u << 27) | (1u << 23) | (1u << 28)), 0u);
    CHECK_EQ(p.air_dismount_revert_frames, 17);
    CHECK_EQ(s.air_dismount_revert_frames, 17);
}

S3_TEST(concrete_stance_truth_table_keeps_natural_relative_and_fakie_distinct) {
    PoseBuffers scratch{};
    const auto empty = scratch.matrices.allocate(0);
    const CompletedSetData completed = complete(SetDataQueue::new_(std::move(scratch), 1));
    const ActorPoseBuffers poses{empty.at(0), empty.at(0)};
    for (bool reversed : {false, true}) {
        for (bool mirrored : {false, true}) {
            for (bool fakie : {false, true}) {
                for (bool nose : {false, true}) {
                    SkaterPublicationState s = state();
                    s.orientation_bit31 = reversed;
                    s.mirrored = mirrored;
                    s.riding_fakie = fakie;
                    s.weight_on_nose = nose;
                    s.relative_stance = 1;
                    s.natural_stance = 2;
                    s.signal = std::nullopt;
                    PhysicsPosePacket p = packet();
                    p.bone_count = 0;
                    physics_packet::publish(s, completed, poses, p, 0.0, bytes("")).unwrap();
                    CHECK_EQ(p.board_flipped, reversed != mirrored);
                    CHECK_EQ(p.mirrored, mirrored);
                    CHECK_EQ(p.riding_fakie, fakie);
                    CHECK_EQ(p.weight_forwards, fakie != nose);
                    CHECK(p.riding_switch);
                    CHECK(!p.regular_stance);
                }
            }
        }
    }
}

S3_TEST(unsafe_source_extent_is_rejected_before_consuming_requests) {
    PoseBuffers buffers{};
    const auto pose = buffers.matrices.allocate(2);
    const CompletedSetData completed = complete(SetDataQueue::new_(std::move(buffers), 1));
    SkaterPublicationState s = state();
    PhysicsPosePacket p = packet();
    const ActorPoseBuffers poses{pose.at(0), pose.at(0)};
    const auto r = physics_packet::publish(s, completed, poses, p, 0.1, bytes("signup"));
    CHECK(r.is_err() && r.unwrap_err() == BufferError(BufferError::UninitializedBone{0}));
    CHECK(s.request_bit16 && s.request_bit15 && s.air_dismount_revert_requested);
    CHECK((p.foot_surface_ids == std::array<std::uint32_t, 2>{35, 46}));
}

S3_TEST(signal_hash_uses_signed_bytes_nul_termination_and_high_nibble_fold) {
    CHECK_EQ(physics_packet::signal_hash(bytes("")), 0u);
    CHECK_EQ(physics_packet::signal_hash(bytes(std::string_view("a\0ignored", 9))), 97u);
    const std::array<std::uint8_t, 1> ff = {0xff};
    const std::array<std::uint8_t, 1> x80 = {0x80};
    CHECK_EQ(physics_packet::signal_hash(ff), 0x0ffffe1fu);
    CHECK_EQ(physics_packet::signal_hash(x80), 0x0ffffe60u);
}

S3_TEST(completed_setdata_exports_feed_physics_publication) {
    PoseBuffers buffers{};
    const auto hierarchy_input = buffers.matrices.initialized({matrix(12.0f), matrix(13.0f)});
    const auto local_input = buffers.matrices.initialized({matrix(22.0f), matrix(23.0f)});
    const auto hierarchy_actor = buffers.matrices.allocate(2);
    const auto local_actor = buffers.matrices.allocate(2);
    SetDataQueue q = SetDataQueue::new_(std::move(buffers), 1);
    using MU = InternalUpdate<NativeMatrix>;
    q.enqueue(2, SetDataCommand::Matrix{CopyCommand<NativeMatrix>{MU::Preserve{}, hierarchy_input.at(0), hierarchy_actor.at(0)}},
              NO_DEPS)
        .unwrap();
    q.enqueue(2, SetDataCommand::Matrix{CopyCommand<NativeMatrix>{MU::Preserve{}, local_input.at(0), local_actor.at(0)}},
              NO_DEPS)
        .unwrap();
    const CompletedSetData completed = complete(std::move(q));
    const ActorPoseBuffers poses{hierarchy_actor.at(0), local_actor.at(0)};
    PhysicsPosePacket p = packet();
    SkaterPublicationState s = state();
    physics_packet::publish(s, completed, poses, p, 1.0 / 60.0, bytes("signup")).unwrap();
    CHECK(p.hierarchy[0] == matrix(12.0f) && p.hierarchy[1] == matrix(13.0f));
    CHECK(p.local[0] == matrix(22.0f) && p.local[1] == matrix(23.0f));
}
