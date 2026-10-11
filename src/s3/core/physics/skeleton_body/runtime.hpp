// Port of skate-core/src/physics/skeleton_body/runtime.rs (included via skeleton_body.hpp).
// Actual 26-part ownership and Reset82BD9990 / physical-record publication.
#pragma once
#include <array>
#include <cstddef>

#include "s3/core/physics/assembly.hpp"
#include "s3/core/physics/rigid_body.hpp"
#include "s3/core/physics/skeleton_animation_record.hpp"

namespace s3::physics::skeleton_body::runtime {

using assembly::BodySnapshot;
using rigid_body::RetailSimulationStep;
using skeleton_animation_record::AnimationPartTransform;

class SkeletonBody {
public:
    SkeletonBodyDefinition definition;
    SkeletonPhysicalRecord record;
    AnimationPartTransform animation_to_world{};

    /// `authored` is SkeletonData's initialization pose after forward physical
    /// bone-frame mapping. The actor's native initialization pose producer must
    /// supply it; the current graph's first frame is a separate publication.
    static SkeletonBody new_(SkeletonBodyDefinition definition,
                             const std::array<AnimationPartTransform, ANIMATION_PART_COUNT>& authored,
                             AnimationPartTransform spawn, RetailSimulationStep simulation);

    const std::array<BodySnapshot, PART_COUNT>& bodies() const { return bodies_; }
    /// The shared solver owns these same states; do not maintain a second
    /// kinematic pose array beside the states receiving its corrections.
    std::array<BodySnapshot, PART_COUNT>& bodies_mut() { return bodies_; }

    /// Skeleton::SetPartTransform82BE2A38 updates the live part through
    ///82BD4318 and then retains the ORIGINAL requested physical-record frame.
    ///Only pose and its inertia cache change; solved velocities survive.
    void set_part_transform(std::size_t part, AnimationPartTransform frame);

    std::array<AnimationPartTransform, PART_COUNT> part_transforms() const;

    void publish_physical_record(AnimationPartTransform board);

    /// ApplyDisplacementToPart82BE7788. Despite its name this writes the
    /// rigid body's force accumulator and clears cooldown; it is not a pose
    /// adjustment. GeneralUpdate calls it for all24 animation parts.
    void apply_part_displacement(std::size_t part, std::array<float, 4> displacement);

private:
    std::array<BodySnapshot, PART_COUNT> bodies_{};
};

}  // namespace s3::physics::skeleton_body::runtime

namespace s3::physics::skeleton_body {
using runtime::SkeletonBody;
}  // namespace s3::physics::skeleton_body
