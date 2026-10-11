// Port of skate-core/src/input/pad.rs.
// Native pad publication 82966F30; retained records use their big-endian words.
#include "s3/core/input/pad.hpp"

#include <algorithm>
#include <utility>

namespace s3::input::pad {

Pad Pad::from_records(std::vector<std::array<std::uint32_t, 4>> records) {
    Pad pad;
    pad.count_ = records.size();
    pad.records_ = std::move(records);
    return pad;
}

Pad Pad::from_storage(std::vector<std::array<std::uint32_t, 4>> records, std::size_t count) {
    S3_ASSERT(count <= records.size());
    Pad pad;
    pad.records_ = std::move(records);
    pad.count_ = count;
    return pad;
}

void Pad::update(const float* values, std::size_t len) {
    const std::array<std::uint32_t, 4> zero{};
    if (len > count_) {
        if (len > records_.size()) {
            records_.resize(len, zero);
        }
        std::fill(records_.begin() + std::ptrdiff_t(count_), records_.begin() + std::ptrdiff_t(len), zero);
    }
    count_ = len;
    const std::size_t n = std::min(records_.size(), len);
    for (std::size_t i = 0; i < n; ++i) {
        auto& record = records_[i];
        const float value = values[i];
        record[0] = rs::to_bits(value);
        const std::uint32_t down = value > 0.5f ? 1u : 0u;
        const std::uint32_t held = (record[1] >> 8) & 255u;
        if (std::int32_t(record[3]) < 3) {
            record[3] = record[3] + 1u;
            record[1] &= 0xffffu;
        } else if (down == held) {
            record[1] &= 0xffffu;
        } else {
            record[1] = (record[1] & 255u) | (down << 8) | (down != 0 ? (1u << 24) : (1u << 16));
            record[3] = 0;
        }
        if ((record[1] & 0xff00u) != 0) {
            if (record[2] == 0) {
                record[1] = (record[1] & ~255u) | 1u;
                record[2] = 24;
            } else {
                record[2] = record[2] - 1u;
                if (record[2] == 0) {
                    record[1] = (record[1] & ~255u) | 1u;
                    record[2] = 12;
                } else {
                    record[1] &= ~255u;
                }
            }
        } else {
            record[2] = 0;
            record[1] &= ~255u;
        }
    }
}

}  // namespace s3::input::pad
