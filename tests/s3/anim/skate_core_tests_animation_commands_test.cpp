// Port of skate-core/tests/animation_commands.rs (crate integration test).
#include <vector>

#include "harness.hpp"
#include "s3/core/animation/commands.hpp"

using namespace s3::animation::commands;
using batch::JobId;
using batch::QueueError;
using batch::SetDataQueue;
using buffers::BoneSlice;
using buffers::BufferError;
using buffers::PoseBuffers;
using s3::animation::output::NativeMatrix;
using s3::animation::output::Sqt;
using set_data::CopyCommand;
using set_data::InternalUpdate;
using set_data::resolve_set_count;
using set_data::SetDataCommand;
using MUpdate = InternalUpdate<NativeMatrix>;
using OptJob = std::optional<JobId>;

namespace {
NativeMatrix matrix(float v) { return NativeMatrix{{{v, v, v, v}, {v, v, v, v}, {v, v, v, v}, {v, v, v, v}}}; }
SetDataQueue queue(PoseBuffers buffers, std::uint16_t limit) { return SetDataQueue::new_(std::move(buffers), limit); }
SetDataCommand mcmd(MUpdate update, BoneSlice<NativeMatrix> internal, std::optional<BoneSlice<NativeMatrix>> external) {
    return SetDataCommand::Matrix{CopyCommand<NativeMatrix>{update, internal, external}};
}
MUpdate set(BoneSlice<NativeMatrix> source, std::uint8_t count) { return MUpdate::Set{source, count}; }
const MUpdate PRESERVE = MUpdate::Preserve{};
const std::vector<OptJob> NO_DEPS{};
batch::CompletedSetData complete_or_panic(SetDataQueue& q) {
    auto r = q.submit().complete();
    if (r.is_err()) S3_PANIC("set data execution failed");
    return std::move(r.unwrap());
}
}  // namespace

S3_TEST(partial_set_exports_whole_skeleton_and_preserves_tail) {
    PoseBuffers buffers{};
    const auto source = buffers.matrices.initialized({matrix(9.0f)});
    const auto internal = buffers.matrices.initialized({matrix(1.0f), matrix(2.0f), matrix(3.0f)});
    const auto output = buffers.matrices.initialized(std::vector<NativeMatrix>(4, matrix(-1.0f)));
    SetDataQueue q = queue(std::move(buffers), 4);
    q.enqueue(3, mcmd(set(source.at(0), 1), internal.at(0), output.at(0)), NO_DEPS).unwrap();
    CHECK(q.flush());
    CHECK(!q.flush());
    const auto done = complete_or_panic(q);
    CHECK((done.buffers().matrices.read(output.at(0), 4).unwrap() ==
           std::vector<NativeMatrix>{matrix(9.0f), matrix(2.0f), matrix(3.0f), matrix(-1.0f)}));
}

S3_TEST(alias_writes_feed_later_records_and_final_external_export) {
    PoseBuffers buffers{};
    const auto source = buffers.matrices.initialized({matrix(7.0f)});
    const auto scratch = buffers.matrices.initialized(std::vector<NativeMatrix>(2, matrix(0.0f)));
    const auto output = buffers.matrices.allocate(1);
    SetDataQueue q = queue(std::move(buffers), 8);
    q.enqueue(1, mcmd(set(source.at(0), 1), scratch.at(0), scratch.at(1)), NO_DEPS).unwrap();
    q.enqueue(1, mcmd(PRESERVE, scratch.at(1), output.at(0)), NO_DEPS).unwrap();
    const auto done = complete_or_panic(q);
    CHECK((done.buffers().matrices.read(output.at(0), 1).unwrap() == std::vector<NativeMatrix>{matrix(7.0f)}));
}

S3_TEST(uninitialized_tail_does_not_become_a_valid_zero_pose) {
    PoseBuffers buffers{};
    const auto source = buffers.matrices.initialized({matrix(8.0f)});
    const auto internal = buffers.matrices.allocate(2);
    const auto output = buffers.matrices.initialized(std::vector<NativeMatrix>(2, matrix(3.0f)));
    SetDataQueue q = queue(std::move(buffers), 8);
    q.enqueue(2, mcmd(set(source.at(0), 1), internal.at(0), output.at(0)), NO_DEPS).unwrap();
    const auto done = complete_or_panic(q);
    const auto r = done.buffers().matrices.read(output.at(0), 2);
    CHECK(r.is_err() && r.unwrap_err() == BufferError(BufferError::UninitializedBone{1}));
    CHECK((done.buffers().matrices.read(output.at(0), 1).unwrap() == std::vector<NativeMatrix>{matrix(8.0f)}));
}

S3_TEST(sqt_copy_preserves_every_float_bit_and_nonzero_bone_offset) {
    PoseBuffers buffers{};
    Sqt value;
    value.scale = {s3::rs::from_bits(0x7fc12345u), -0.0f, 3.0f, 4.0f};
    value.rotation = {5.0f, 5.0f, 5.0f, 5.0f};
    value.translation = {6.0f, 6.0f, 6.0f, 6.0f};
    const auto source = buffers.sqt.initialized({value});
    const auto output = buffers.sqt.allocate(3);
    SetDataQueue q = queue(std::move(buffers), 1);
    q.enqueue(1,
              SetDataCommand::Sqt{CopyCommand<Sqt>{InternalUpdate<Sqt>::Preserve{}, source.at(0), output.at(1)}},
              NO_DEPS)
        .unwrap();
    const auto done = complete_or_panic(q);
    const Sqt copied = done.buffers().sqt.read(output.at(1), 1).unwrap()[0];
    CHECK(s3::rs::to_bits(copied.scale) == s3::rs::to_bits(value.scale));
    CHECK(copied.rotation == value.rotation);
    CHECK(copied.translation == value.translation);
    CHECK(done.buffers().sqt.read(output.at(0), 1).is_err());
}

S3_TEST(capacity_boundary_and_dependency_preserve_producer_before_consumer) {
    PoseBuffers buffers{};
    const auto input = buffers.matrices.initialized({matrix(42.0f)});
    const auto intermediate = buffers.matrices.allocate(1);
    const auto output = buffers.matrices.allocate(1);
    SetDataQueue q = queue(std::move(buffers), 1);
    const std::vector<OptJob> none = {std::nullopt};
    const JobId first = q.enqueue(1, mcmd(PRESERVE, input.at(0), intermediate.at(0)), none).unwrap();
    const std::vector<OptJob> deps = {first, std::nullopt, first};
    const JobId second = q.enqueue(1, mcmd(PRESERVE, intermediate.at(0), output.at(0)), deps).unwrap();
    CHECK(first != second);
    const auto done = complete_or_panic(q);
    CHECK((done.buffers().matrices.read(output.at(0), 1).unwrap() == std::vector<NativeMatrix>{matrix(42.0f)}));
}

S3_TEST(prepare_and_cancel_never_execute_copies) {
    PoseBuffers buffers{};
    const auto source = buffers.matrices.initialized({matrix(9.0f)});
    const auto output = buffers.matrices.initialized({matrix(1.0f)});
    SetDataQueue q = queue(std::move(buffers), 1);
    q.enqueue(1, mcmd(PRESERVE, source.at(0), output.at(0)), NO_DEPS).unwrap();
    q.flush();
    CHECK((q.cancel().matrices.read(output.at(0), 1).unwrap() == std::vector<NativeMatrix>{matrix(1.0f)}));
}

S3_TEST(unresolved_partial_overlap_fails_without_undoing_prior_set) {
    PoseBuffers buffers{};
    const auto source = buffers.matrices.initialized({matrix(8.0f)});
    const auto internal = buffers.matrices.initialized(std::vector<NativeMatrix>(3, matrix(0.0f)));
    SetDataQueue q = queue(std::move(buffers), 1);
    q.enqueue(2, mcmd(set(source.at(0), 1), internal.at(0), internal.at(1)), NO_DEPS).unwrap();
    auto result = q.submit().complete();
    if (result.is_ok()) S3_PANIC("overlap accepted");
    const auto& failure = result.unwrap_err();
    CHECK(failure.cause == BufferError(BufferError::UnsupportedPartialOverlap{}));
    CHECK((failure.buffers.matrices.read(internal.at(0), 3).unwrap() ==
           std::vector<NativeMatrix>{matrix(8.0f), matrix(0.0f), matrix(0.0f)}));
}

S3_TEST(latest_enqueued_skeleton_count_applies_to_every_command_in_batch) {
    PoseBuffers buffers{};
    const auto input = buffers.matrices.initialized({matrix(1.0f), matrix(2.0f)});
    const auto output = buffers.matrices.allocate(2);
    SetDataQueue q = queue(std::move(buffers), 3);
    const JobId first = q.enqueue(1, mcmd(PRESERVE, input.at(0), output.at(0)), NO_DEPS).unwrap();
    const std::vector<OptJob> deps = {first};
    q.enqueue(2, mcmd(PRESERVE, input.at(0), std::nullopt), deps).unwrap();
    const auto done = complete_or_panic(q);
    CHECK((done.buffers().matrices.read(output.at(0), 2).unwrap() ==
           std::vector<NativeMatrix>{matrix(1.0f), matrix(2.0f)}));
    CHECK_EQ(resolve_set_count(0, 258), std::uint8_t(2));
    CHECK_EQ(resolve_set_count(0, 256), std::uint8_t(0));
    CHECK_EQ(resolve_set_count(255, 20), std::uint8_t(255));
}

S3_TEST(foreign_buffer_and_job_handles_are_rejected) {
    PoseBuffers foreign{};
    const auto foreign_pose = foreign.matrices.initialized({matrix(9.0f)});
    SetDataQueue foreign_queue = queue(std::move(foreign), 2);
    const JobId foreign_job = foreign_queue.enqueue(1, mcmd(PRESERVE, foreign_pose.at(0), std::nullopt), NO_DEPS).unwrap();
    PoseBuffers buffers{};
    const auto local = buffers.matrices.initialized({matrix(1.0f)});
    {
        const auto r = buffers.matrices.read(foreign_pose.at(0), 1);
        CHECK(r.is_err() && r.unwrap_err() == BufferError(BufferError::UnknownBuffer{}));
    }
    SetDataQueue own_queue = queue(std::move(buffers), 2);
    const SetDataCommand command = mcmd(PRESERVE, local.at(0), std::nullopt);
    const std::vector<OptJob> deps = {foreign_job};
    {
        const auto r = own_queue.enqueue(1, command, deps);
        CHECK(r.is_err() && r.unwrap_err() == QueueError::UnavailableDependency);
    }
    own_queue.enqueue(1, command, NO_DEPS).unwrap();
    CHECK(own_queue.submit().complete().is_ok());
}

S3_TEST(full_export_count_is_not_truncated_to_the_set_byte) {
    PoseBuffers buffers{};
    std::vector<NativeMatrix> values_in;
    for (int i = 0; i < 256; ++i) values_in.push_back(matrix(static_cast<float>(i)));
    const auto internal = buffers.matrices.initialized(values_in);
    const auto empty_source = buffers.matrices.allocate(0);
    const auto output = buffers.matrices.allocate(256);
    SetDataQueue q = queue(std::move(buffers), 1);
    q.enqueue(256, mcmd(set(empty_source.at(SIZE_MAX), 0), internal.at(0), output.at(0)), NO_DEPS).unwrap();
    const auto done = complete_or_panic(q);
    const auto values = done.buffers().matrices.read(output.at(0), 256).unwrap();
    CHECK(values[0] == matrix(0.0f));
    CHECK(values[255] == matrix(255.0f));
}
