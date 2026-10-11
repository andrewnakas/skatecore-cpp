// Port of skate-core/src/input/history.rs.
// Native controller history owner (`8296D0D0`, `8296D1F0`, `82699230`).
#include "s3/core/input/history.hpp"

#include <algorithm>

namespace s3::input::history {

HistoryRecord HistoryRecord::new_(const float* values, std::size_t len) {
    S3_ASSERT(len <= MAX_VALUES);
    HistoryRecord record;
    record.count_ = len;
    std::copy(values, values + len, record.values_.begin());
    return record;
}

void PadHistory::publish(const HistoryRecord* records, std::size_t len) {
    S3_ASSERT(len <= DEVICE_SLOTS);
    std::copy(records, records + len, batches[write_].begin());
    write_ = (write_ + 1) % HISTORY_CAPACITY;
}

bool PadHistory::drain_to_latest(std::array<Pad, DEVICE_SLOTS>& pads) {
    if (is_empty()) {
        return false;
    }
    std::size_t latest = read_;
    while ((read_ + 1) % HISTORY_CAPACITY != write_) {
        read_ = (read_ + 1) % HISTORY_CAPACITY;
        latest = read_;
    }
    for (std::size_t i = 0; i < DEVICE_SLOTS; ++i) {
        const HistoryRecord& record = batches[latest][i];
        pads[i].update(record.values_ptr(), record.count());
    }
    return true;
}

}  // namespace s3::input::history
