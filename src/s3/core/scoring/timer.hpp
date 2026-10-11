// Port of skate-core/src/scoring/timer.rs.
// Point-backed countdowns, 82DA4C28 and module82DA37B0.
#pragma once
#include <array>
#include <utility>

#include "s3/core/rs.hpp"

namespace s3::scoring::timer {

struct PointTimer {
    float points = 0.0f;
    bool expired = false;

    /// Returns the native near-one hold result used by the ground collector.
    bool advance(float dt, float drain, float scale, bool hold_near_one);
    void credit(float points_, float capacity) { points = rs::min(points + points_, capacity); }
    float seconds(float drain) const { return points / drain; }
};
S3_DERIVE_EQ(PointTimer, points, expired)

struct ComboTimer {
    PointTimer timer;
    float multiplier = 1.0f;

    /// The multiplier may increase, or decrease when a sufficiently large
    /// new reward authorizes refreshing it. Timer decay alone does not lower it.
    void credit(float reward, float capacity, std::array<std::pair<float, float>, 3> levels, float refresh);
};
S3_DERIVE_EQ(ComboTimer, timer, multiplier)

}  // namespace s3::scoring::timer
