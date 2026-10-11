// Port of skate-core/src/player/offboard/pose_adjust.rs (whole file).
// Skeleton::UpdateOffboardAdjust82BD8F10 and SkeletonIK82BF1A78.
// Both transforms are current animation globals, before physical bone offsets.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

#include "s3/core/physics/skeleton_animation_record.hpp"

namespace s3::player::offboard::pose_adjust {

using Transform = physics::skeleton_animation_record::AnimationPartTransform;

/// Flags2476 bit2 selects left when set, right when clear.
inline std::size_t selected_hand(std::uint32_t flags_2476) { return (flags_2476 & 4u) == 0 ? 1 : 0; }

/// Actual hand composed with the rigid inverse of the board-parented hand.
/// Native inverse is a transpose, not a general scale/shear matrix inverse.
inline Transform adjustment(const Transform& actual, const Transform& reparented) {
    std::array<std::array<float, 4>, 3> inverse{};
    for (std::size_t column = 0; column < 3; ++column) {
        inverse[column] = {reparented[0][column], reparented[1][column], reparented[2][column], 0.0f};
    }
    // 82BF1B18/40/50: inverse translation accumulates Z, then Y, then X.
    const std::array<float, 4> translation = rs::from_fn<4>([&](std::size_t lane) {
        const float z = (0.0f - reparented[3][2]) * inverse[2][lane];
        const float yz = rs::mul_add(0.0f - reparented[3][1], inverse[1][lane], z);
        return rs::mul_add(0.0f - reparented[3][0], inverse[0][lane], yz);
    });
    Transform out{};
    for (std::size_t column = 0; column < 4; ++column) {
        for (std::size_t lane = 0; lane < 4; ++lane) {
            if (column < 3) {
                const float x = inverse[column][0] * actual[0][lane];
                const float y = rs::mul_add(inverse[column][1], actual[1][lane], x);
                out[column][lane] = rs::mul_add(inverse[column][2], actual[2][lane], y);
            } else {
                const float x = rs::mul_add(translation[0], actual[0][lane], actual[3][lane]);
                const float y = rs::mul_add(translation[1], actual[1][lane], x);
                out[column][lane] = rs::mul_add(translation[2], actual[2][lane], y);
            }
        }
    }
    return out;
}

}  // namespace s3::player::offboard::pose_adjust
