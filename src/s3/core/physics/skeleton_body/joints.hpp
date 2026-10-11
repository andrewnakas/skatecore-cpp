// Port of skate-core/src/physics/skeleton_body/joints.rs (included via skeleton_body.hpp).
// SkeletonBody::InitJoints82BE5B30 and SetUpNormalJointLimits82BE58A8.
// Frames attach the child to its nearest physical ancestor; the initial
// animation bone poses and authored volume frames are distinct inputs.
#pragma once
#include <array>
#include <cstddef>
#include <optional>
#include <vector>

#include "s3/core/physics/assembly.hpp"
#include "s3/core/physics/joint_records.hpp"
#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/core/physics/solver.hpp"

namespace s3::physics::skeleton_body::joints {

using assembly::BodySnapshot;
using joint_records::RetailJointFramesRaw;
using joint_records::RetailJointParametersRaw;
using skeleton_animation_record::AnimationPartTransform;
using solver::JointConstraint;

struct JointBone {
    std::array<float, 4> parent_orientation{};
    std::array<float, 4> joint_orientation{};
    AnimationPartTransform volume_frame{};
    float swing_limit = 0.0f;
    float twist_limit = 0.0f;
};

struct JointSettings {
    bool ball_joint = false;
    float swing_angle = 0.0f;
    float twist_angle = 0.0f;
    float swing_ragdoll = 0.0f;
    float twist_ragdoll = 0.0f;
};

struct SkeletonJointSettings {
    std::array<float, 4> displacement_limit{};
    float twist_displacement_limit = 0.0f;
    float swing_displacement_limit = 0.0f;
    bool enforce_swing_free = false;
    bool enforce_twist_free = false;
};

struct SkeletonJoint {
    std::size_t parent = 0;
    std::size_t child = 0;
    RetailJointParametersRaw parameters;
    RetailJointFramesRaw frames;
};

struct SkeletonJoints {
    std::array<SkeletonJoint, JOINT_COUNT> records{};

    static rs::Result<SkeletonJoints, const char*> new_(
        const std::array<AnimationPartTransform, ANIMATION_PART_COUNT>& initial_bones,
        const std::array<std::optional<std::size_t>, ANIMATION_PART_COUNT>& parents,
        const std::array<JointBone, ANIMATION_PART_COUNT>& bones,
        const std::array<JointSettings, JOINT_COUNT>& settings, SkeletonJointSettings global);

    /// Append these to the same constraint pass as board contacts/joints/drives.
    /// Reaction indices are supplied by the unified simulation owner.
    std::vector<JointConstraint> build(const std::array<BodySnapshot, PART_COUNT>& bodies,
                                       std::size_t reaction_base, float time_step) const;
};

}  // namespace s3::physics::skeleton_body::joints

namespace s3::physics::skeleton_body {
using joints::JointBone;
using joints::JointSettings;
using joints::SkeletonJoint;
using joints::SkeletonJoints;
using joints::SkeletonJointSettings;
}  // namespace s3::physics::skeleton_body
