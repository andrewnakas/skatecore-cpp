// Port of skate-core/src/animation/commands/batch.rs.
// Owned host execution for the recovered SetData worker.
//
// 828D7E08/828D7FD8 define batch boundaries and dependency collection; preparing
// a job is not submitting or completing it (828C88D8). This synchronous host
// adapter accepts only SetData jobs and already-materialized input buffers.
// It is not a replacement for the full ACS scheduler or its other engines.
//
// Rust's consuming `self` methods (submit, cancel, complete, into_buffers) move out of
// `*this`; the object must not be used afterwards.
#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "s3/core/animation/commands/buffers.hpp"
#include "s3/core/animation/commands/set_data.hpp"
#include "s3/core/animation/slice.hpp"

namespace s3::animation::commands::batch {

using buffers::BufferError;
using buffers::PoseBuffers;
using set_data::SetDataCommand;

struct JobId {
    // Private in Rust.
    std::size_t owner = 0;
    std::size_t index = 0;
};
S3_DERIVE_EQ(JobId, owner, index)

enum class QueueError {
    CommandCapacityExceeded,
    DependencyCapacityExceeded,
    /// Other engines must finish and supply their owned buffers before this
    /// SetData-only adapter is constructed; unresolved handles are rejected.
    UnavailableDependency,
};

/// Private in Rust.
struct Job {
    std::uint16_t bone_count = 0;
    std::vector<SetDataCommand> commands;
    std::vector<JobId> dependencies;
    static Job new_() { return Job{}; }
};

class CompletedSetData;
struct ExecutionFailure;
class PendingSetData;

class SetDataQueue {
public:
    /// `max_work_units` is Rust's NonZeroU16; zero panics.
    static SetDataQueue new_(PoseBuffers buffers, std::uint16_t max_work_units);

    /// A returned job identity establishes neither execution nor output readiness.
    /// None corresponds to a null dependency; current-job references are removed.
    rs::Result<JobId, QueueError> enqueue(std::uint16_t bone_count, SetDataCommand command,
                                          Slice<const std::optional<JobId>> dependencies);

    /// Finalize pending metadata; never executes a worker or exposes its output.
    bool flush();

    /// Consumes the queue.
    PendingSetData submit();

    /// Safe because no workers have been started by this host adapter. Consumes the queue.
    PoseBuffers cancel();

private:
    SetDataQueue(std::size_t owner, PoseBuffers buffers, std::uint16_t max_work_units)
        : owner_(owner), buffers_(std::move(buffers)), max_work_units_(max_work_units) {}
    std::size_t owner_;
    PoseBuffers buffers_;
    std::uint16_t max_work_units_;
    std::vector<Job> prepared_;
    Job current_;
    std::size_t command_count_ = 0;
};

struct ExecutionFailure {
    /// Diagnostic/recovery storage only; this is not a completed pose certificate.
    PoseBuffers buffers;
    JobId job;
    BufferError cause;
};

class CompletedSetData {
public:
    explicit CompletedSetData(PoseBuffers buffers) : buffers_(std::move(buffers)) {}
    const PoseBuffers& buffers() const { return buffers_; }
    /// Reuse can begin only after completion releases ownership. Consumes `*this`.
    PoseBuffers into_buffers() { return std::move(buffers_); }

private:
    PoseBuffers buffers_;
};

class PendingSetData {
public:
    PendingSetData(std::size_t owner, PoseBuffers buffers, std::vector<Job> jobs)
        : owner_(owner), buffers_(std::move(buffers)), jobs_(std::move(jobs)) {}

    /// Execute complete workers in preparation order. All accepted dependencies
    /// point to earlier jobs, so their writes precede dependent reads. No native
    /// concurrency ordering beyond those dependencies is asserted. Consumes `*this`.
    rs::Result<CompletedSetData, ExecutionFailure> complete();

private:
    std::size_t owner_;
    PoseBuffers buffers_;
    std::vector<Job> jobs_;
};

}  // namespace s3::animation::commands::batch
