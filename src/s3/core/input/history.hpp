// Port of skate-core/src/input/history.rs.
// Native controller history owner (`8296D0D0`, `8296D1F0`, `82699230`).
//
// The cache is thirty frame batches. Each batch contains four 100-byte
// records, one per registered device. A record contains a value count and
// up to 24 native float values. Startup is read=0 and write=1.
#pragma once
#include <array>
#include <cstddef>
#include <initializer_list>
#include <vector>

#include "s3/core/input/pad.hpp"
#include "s3/core/rs.hpp"

namespace s3::input::history {

using pad::Pad;

inline constexpr std::size_t HISTORY_CAPACITY = 30;
inline constexpr std::size_t DEVICE_SLOTS = 4;
inline constexpr std::size_t MAX_VALUES = 24;

struct HistoryRecord {
    // Private in Rust; public here so the ported tests can inspect them.
    std::array<float, MAX_VALUES> values_{};
    std::size_t count_ = 0;

    static HistoryRecord new_(const float* values, std::size_t len);
    static HistoryRecord new_(std::initializer_list<float> values) {
        return new_(values.begin(), values.size());
    }
    std::size_t count() const { return count_; }
    /// `&values[..count]`.
    std::vector<float> values() const {
        return std::vector<float>(values_.begin(), values_.begin() + std::ptrdiff_t(count_));
    }
    const float* values_ptr() const { return values_.data(); }
    /// Failed device polls clear publication count while retaining value words.
    void clear_count() { count_ = 0; }
};
S3_DERIVE_EQ(HistoryRecord, values_, count_)

struct PadHistory {
    // Private in Rust; public here so the ported tests can inspect them.
    std::array<std::array<HistoryRecord, DEVICE_SLOTS>, HISTORY_CAPACITY> batches{};
    std::size_t read_ = 0;
    std::size_t write_ = 1;

    /// Matches `826986A8`: zeroed storage, read index 0, write index 1.
    static PadHistory new_() { return PadHistory{}; }
    std::size_t read_index() const { return read_; }
    std::size_t write_index() const { return write_; }

    /// `8296D0D0` copies the registered-device prefix into one frame batch,
    /// then advances the producer once. Uncopied slots retain their contents;
    /// an empty poll still advances the producer.
    void publish(const HistoryRecord* records, std::size_t len);
    template <std::size_t N>
    void publish(const std::array<HistoryRecord, N>& records) {
        publish(records.data(), N);
    }
    void publish(const std::vector<HistoryRecord>& records) { publish(records.data(), records.size()); }

    /// `82699230` drains to the newest available batch and updates all Pads
    /// exactly once with that batch.
    bool drain_to_latest(std::array<Pad, DEVICE_SLOTS>& pads);

    bool is_empty() const { return (read_ + 1) % HISTORY_CAPACITY == write_; }
};
S3_DERIVE_EQ(PadHistory, batches, read_, write_)

}  // namespace s3::input::history
