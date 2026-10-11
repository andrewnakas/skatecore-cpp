// Port of skate-core/src/physics/skeleton_body/errors.rs (included via skeleton_body.hpp).
// Original physical pose errors82BEBEA8 and normal collision response82BE2438.
#pragma once
#include <array>

#include "s3/core/physics/skeleton_animation_record.hpp"

namespace s3::physics::skeleton_body::errors {

using collision_feedback::V;
using skeleton_animation_record::AnimationPartTransform;

struct SkeletonNormalError {
    /// Skeleton16272: physical error constrained by actual contact planes.
    V impulse{};
    /// Normal82BE2438 clears Skeleton16288 and16304.
    std::array<V, 2> extra{};
    /// Skeleton16384, length before contact-plane filtering.
    float maximum_error = 0.0f;
};

struct SkeletonPoseErrors {
    /// SkeletonState4752, one vector per animated part.
    std::array<V, 24> parts{};
    /// SkeletonState5136/5152: the two additional physical target errors.
    std::array<V, 2> extra{};
    /// SkeletonState5168/5184/5200, written by UpdateExtraTargets.
    std::array<V, 3> targets{};

    ///82BEBBB0 clears only target histories. The next82BEBEA8 replaces errors.
    void reset_history();
    void set_targets(ExtraTargetPositions positions);
    ///82BEBEA8. These are actual part-frame errors, not COM or force proxies.
    void update(const SkeletonPhysicalRecord& physical, AnimationPartTransform animation_to_world,
                const std::array<AnimationPartTransform, 24>& drive_frames);
    ///82BE2438. Strict comparison and the first raw-pair tie behavior differ
    /// from the filtered-pair tie; preserve both selection sequences.
    SkeletonNormalError normal_response(const SkeletonCollisionFeedback& collision, V support_normal) const;
    ///82BD9FB0 partial-ragdoll branch copies the two extra errors directly,
    /// then82BE2310 computes the same raw eight-part maximum length.
    SkeletonNormalError partial_response() const;

private:
    float maximum_error() const;
};

}  // namespace s3::physics::skeleton_body::errors

namespace s3::physics::skeleton_body {
using errors::SkeletonNormalError;
using errors::SkeletonPoseErrors;
}  // namespace s3::physics::skeleton_body
