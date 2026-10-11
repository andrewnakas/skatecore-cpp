// Port of skate-core/src/scoring/carrier.rs.
// Scorable carrier lifecycle, TU3 82DA45C8/46D0/5DE0/5F98.
// Collectors supply the native clock and already-conditioned point factor.
#pragma once
#include <cstdint>

#include "s3/core/scoring.hpp"

namespace s3::scoring::carrier {

/// 82DA45C8 multiplies the sum of authored and collector delay by the
/// binary32 constant at 8303745C (0x42700000), then truncates to integer.
std::uint32_t delay_ticks(float authored_seconds, float extra_seconds);

struct Carrier {
    Scorable scorable;
    std::int32_t points = 0;
    float factor = 0.0f;
    float reward = 0.0f;
    float announcement_threshold = 0.0f;
    std::uint32_t start_tick = 0;
    std::uint32_t delay_ticks = 0;
    bool announced = false;
    bool completed = false;
    bool unannounced = false;
    bool switch_ = false;  // Rust field `switch` (C++ keyword)
    bool fakie = false;

    /// Delay is supplied in native ticks: simulation frame count must not be
    /// substituted for the source clock's frequency.
    static Carrier new_(Scorable scorable, std::int32_t points, float factor, float announcement_threshold,
                        std::uint32_t start_tick, std::uint32_t delay_ticks, bool switch_, bool fakie);

    /// Returns true only on the announcement edge. Unsigned subtraction is
    /// intentional and preserves the executable's behavior at clock rollover.
    bool announce(std::uint32_t tick, float unannounced_factor);

    /// Called once by the collector when removing its current carrier. The
    /// executable does not mark an early completion as an announcement.
    bool complete(float unannounced_factor);

    /// Replace a carrier through its conversion link (82DA6320). Conversion
    /// reports the previous carrier as completed and credits the replacement
    /// immediately, even before its normal announcement delay.
    void convert_to(Carrier& replacement, float unannounced_factor);

private:
    void credit(float unannounced_factor);
};

}  // namespace s3::scoring::carrier
