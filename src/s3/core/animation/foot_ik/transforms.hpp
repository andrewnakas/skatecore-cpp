// Port of skate-core/src/animation/foot_ik/transforms.rs.
// Animated portion of CalculateInitialPartTransforms82BEDF08.
//
// PARTIAL PORT (stage C): only items used by physics (LimbBinding, LimbFrames, inverse_rigid).
// prepare_animation_target needs foot_ik::status and is left to the animation stage.
#pragma once
#include <cstddef>
#include <optional>

#include "s3/core/physics/skeleton_animation_record.hpp"

namespace s3::animation::foot_ik::transforms {

using Transform = physics::skeleton_animation_record::AnimationPartTransform;
using physics::skeleton_animation_record::IDENTITY;

struct LimbBinding {
    /// SkeletonIK1120: the physics volume driven by this target.
    std::size_t part = 0;
    /// SkeletonIK1136: optional adjacent volume receiving the same adjustment.
    std::optional<std::size_t> parent_part;
};

struct LimbFrames {
    // 82BED780 initializes every retained affine frame to identity.
    /// SkeletonIK864, from the actual reparented animation target.
    Transform target = IDENTITY;
    /// SkeletonIK1152; animated world frame, then blended/contact-adjusted.
    Transform world = IDENTITY;
    /// SkeletonIK1408: externally supplied target in world space.
    Transform external_world = IDENTITY;
    /// SkeletonIK1664: target relative to the animated board.
    Transform board = IDENTITY;
    /// SkeletonIK2240, optional adjacent volume in world space.
    Transform parent_world = IDENTITY;
    /// SkeletonIK2496: adjacent volume's external target.
    Transform external_parent_world = IDENTITY;
    /// SkeletonIK2752: adjacent volume relative to the animated board.
    Transform parent_board = IDENTITY;
    /// SkeletonIK3120: animated target lies within the cached contact box.
    bool within_contact_bounds = false;

    static LimbFrames default_() { return LimbFrames{}; }
};

/// 82BEDF5C..DFEC and E1B4..E260: rigid inverse, transpose XYZ and zero W;
/// translation uses negative position with Z multiply, Y FMA, then X FMA.
/// This deliberately does not introduce a general scale/shear inverse.
inline Transform inverse_rigid(const Transform& source) {
    Transform result = IDENTITY;
    for (std::size_t axis = 0; axis < 3; ++axis) {
        result[axis] = {source[0][axis], source[1][axis], source[2][axis], 0.0f};
    }
    std::array<float, 4> translation{};
    for (std::size_t i = 0; i < 4; ++i) translation[i] = 0.0f - source[3][i];
    for (std::size_t i = 0; i < 4; ++i) {
        const float z = translation[2] * result[2][i];
        const float y = rs::mul_add(translation[1], result[1][i], z);
        result[3][i] = rs::mul_add(translation[0], result[0][i], y);
    }
    return result;
}

}  // namespace s3::animation::foot_ik::transforms
