// Port of skate-core/src/animation/commands/batch.rs.
#include "s3/core/animation/commands/batch.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace s3::animation::commands::batch {

SetDataQueue SetDataQueue::new_(PoseBuffers buffers, std::uint16_t max_work_units) {
    if (max_work_units == 0) S3_PANIC("NonZeroU16 max_work_units");
    return SetDataQueue(buffers::new_owner(), std::move(buffers), max_work_units);
}

rs::Result<JobId, QueueError> SetDataQueue::enqueue(std::uint16_t bone_count, SetDataCommand command,
                                                    Slice<const std::optional<JobId>> dependencies) {
    if (command_count_ == static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max())) {
        return rs::make_err(QueueError::CommandCapacityExceeded);
    }
    const bool full = current_.commands.size() == static_cast<std::size_t>(max_work_units_);
    const JobId id{owner_, prepared_.size() + (full ? 1u : 0u)};
    std::vector<JobId> collected = full ? std::vector<JobId>{} : current_.dependencies;
    for (const std::optional<JobId>& maybe : dependencies) {
        if (!maybe) continue;
        const JobId dependency = *maybe;
        if (dependency == id) continue;
        if (dependency.owner != owner_ || dependency.index >= id.index) {
            return rs::make_err(QueueError::UnavailableDependency);
        }
        if (std::find(collected.begin(), collected.end(), dependency) == collected.end()) {
            if (collected.size() == static_cast<std::size_t>(std::numeric_limits<std::uint8_t>::max())) {
                return rs::make_err(QueueError::DependencyCapacityExceeded);
            }
            collected.push_back(dependency);
        }
    }
    if (full) flush();
    // The native job header is rewritten on EVERY enqueue. Its last count
    // applies to external copies for all records in that job.
    current_.bone_count = bone_count;
    current_.commands.push_back(std::move(command));
    current_.dependencies = std::move(collected);
    command_count_ += 1;
    return id;
}

bool SetDataQueue::flush() {
    if (current_.commands.empty()) return false;
    prepared_.push_back(std::exchange(current_, Job::new_()));
    return true;
}

PendingSetData SetDataQueue::submit() {
    flush();
    return PendingSetData(owner_, std::move(buffers_), std::move(prepared_));
}

PoseBuffers SetDataQueue::cancel() { return std::move(buffers_); }

rs::Result<CompletedSetData, ExecutionFailure> PendingSetData::complete() {
    for (std::size_t index = 0; index < jobs_.size(); ++index) {
        const Job& job = jobs_[index];
        auto result = set_data::execute(buffers_, job.bone_count, job.commands);
        if (result.is_err()) {
            return rs::make_err(ExecutionFailure{std::move(buffers_), JobId{owner_, index}, result.unwrap_err()});
        }
    }
    return CompletedSetData(std::move(buffers_));
}

}  // namespace s3::animation::commands::batch
