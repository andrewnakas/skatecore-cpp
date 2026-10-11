// Port of skate-core/src/player/selector/conditions.rs (whole file).
// Small predicates called by TU3 `CalcSuggestedState` (`0x82D8ADE8`).
#pragma once
#include <cstdint>

#include "s3/core/rs.hpp"

namespace s3::player::selector::conditions {

struct BoardBodyState {
    /// `SkateboardBody + 856`.
    float field_856 = 0.0f;
    /// `SkateboardBody + 7692`.
    float field_7692 = 0.0f;
};
S3_DERIVE_EQ(BoardBodyState, field_856, field_7692)

struct TwoStageThresholds {
    float field_856_primary = 0.0f;
    float field_856_secondary = 0.0f;
    float field_7692 = 0.0f;
};
S3_DERIVE_EQ(TwoStageThresholds, field_856_primary, field_856_secondary, field_7692)

/// TU3 `0x82D8BBB8`. All comparisons are strict, matching `fcmpu`/`bgt`.
inline bool condition_is_off_ground_skitching(BoardBodyState body, TwoStageThresholds thresholds) {
    return body.field_856 > thresholds.field_856_primary ||
           (body.field_856 > thresholds.field_856_secondary && body.field_7692 > thresholds.field_7692);
}

/// TU3 `0x82D8BD50`. The separate settings source is intentional: the native
/// function reads these three values from the live physics-mode collection.
inline bool condition_is_off_ground(BoardBodyState body, TwoStageThresholds thresholds) {
    return body.field_856 > thresholds.field_856_primary ||
           (body.field_856 > thresholds.field_856_secondary && body.field_7692 > thresholds.field_7692);
}

struct SkeletonAnimationState {
    /// `Skeleton + 16420`.
    std::uint32_t mode_16420 = 0;
    /// Lane one of the vector at `Skeleton + 12624 + 32`.
    float back_chain_lane_12656 = 0.0f;
    /// `*(Skeleton + 24) + 800`.
    float threshold_800 = 0.0f;
};
S3_DERIVE_EQ(SkeletonAnimationState, mode_16420, back_chain_lane_12656, threshold_800)

/// TU3 `0x82BDC6C8`. The native absolute value clears the scalar sign bit.
inline bool is_skateboard_animated(SkeletonAnimationState input) {
    if (input.mode_16420 == 2) {
        return true;
    }
    const float magnitude = rs::from_bits(rs::to_bits(input.back_chain_lane_12656) & 0x7fffffffu);
    if (magnitude <= input.threshold_800) {
        return false;
    }
    return input.mode_16420 != 1;
}

}  // namespace s3::player::selector::conditions
