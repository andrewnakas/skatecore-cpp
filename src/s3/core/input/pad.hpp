// Port of skate-core/src/input/pad.rs.
// Native pad publication 82966F30; retained records use their big-endian words.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "s3/core/rs.hpp"

namespace s3::input::pad {

class Pad {
public:
    /// An empty native cInputPad. 8296D288/826986A8 initialize the published
    /// count to zero; records become initialized only when a larger count is
    /// published by 82966F30.
    static Pad new_() { return Pad{}; }

    /// Existing initialized native records. The native count is their length.
    static Pad from_records(std::vector<std::array<std::uint32_t, 4>> records);
    static Pad from_storage(std::vector<std::array<std::uint32_t, 4>> records, std::size_t count);
    std::size_t count() const { return count_; }
    const std::vector<std::array<std::uint32_t, 4>>& records() const { return records_; }

    /// Complete update, including growth reset, updated count on shorter
    /// polls, three-call edge suppression and 24/12-call repeat intervals.
    void update(const float* values, std::size_t len);
    void update(const std::vector<float>& values) { update(values.data(), values.size()); }

    bool operator==(const Pad& o) const { return records_ == o.records_ && count_ == o.count_; }
    bool operator!=(const Pad& o) const { return !(*this == o); }

private:
    std::vector<std::array<std::uint32_t, 4>> records_;
    std::size_t count_ = 0;
};

}  // namespace s3::input::pad
