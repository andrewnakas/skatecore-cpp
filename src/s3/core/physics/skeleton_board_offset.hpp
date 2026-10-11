// Port of skate-core/src/physics/skeleton_board_offset.rs.
// Complete Skeleton::UpdateSkateboardOffsetTransform82BDD630 and
// SkeletonIK::AddOffsetTransform82BF1C68. Counters are animation updates.
#pragma once
#include <array>

#include "s3/core/physics/skeleton_animation_record.hpp"

namespace s3::physics::skeleton_board_offset {

using skeleton_animation_record::AnimationPartTransform;

struct SkateboardOffset {
    // Default: Skeleton constructor82BD76E8..7708 and82BD79A0..79BC.
    AnimationPartTransform transform = skeleton_animation_record::IDENTITY;
    float orientation_frames = 0.0f;
    float height_frames = 0.0f;
    bool orientation_refreshed = false;
    bool height_refreshed = false;

    static SkateboardOffset default_() { return SkateboardOffset{}; }

    /// The offboard, landing-on-board and grind-air producers refresh both
    /// channels for15 updates after writing their independently calculated frame.
    void refresh_transform(AnimationPartTransform transform);

    /// LandingAdjust writes only Y and its own duration, preserving X/Z/basis.
    void refresh_height(float height, float frames);

    /// Update decay first, then apply the resulting offset to the board and
    /// four IK targets. Positive counters select application before decrement.
    void update(AnimationPartTransform& board_pose, std::array<AnimationPartTransform, 4>& targets);
};

}  // namespace s3::physics::skeleton_board_offset
