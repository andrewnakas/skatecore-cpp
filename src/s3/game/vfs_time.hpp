// skate_vfs::time::Duration (re-export of std/web_time Duration): only the parts the
// simulation uses (nanosecond construction, comparison).
#pragma once
#include <cstdint>

namespace s3::game::vfs_time {

struct Duration {
    std::uint64_t secs = 0;
    std::uint32_t nanos = 0;

    static constexpr Duration from_nanos(std::uint64_t n) {
        return Duration{n / 1000000000u, static_cast<std::uint32_t>(n % 1000000000u)};
    }
    constexpr std::uint64_t as_nanos() const { return secs * 1000000000u + nanos; }
    constexpr double as_secs_f64() const { return double(secs) + double(nanos) / 1e9; }
};
constexpr bool operator==(const Duration& a, const Duration& b) { return a.secs == b.secs && a.nanos == b.nanos; }
constexpr bool operator!=(const Duration& a, const Duration& b) { return !(a == b); }

}  // namespace s3::game::vfs_time
