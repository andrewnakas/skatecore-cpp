// Port of skate-core/src/physics/skeleton_body/targets.rs (included via skeleton_body.hpp).
// Original TU3 SkeletonDrives::InitHooks82BEAB70 creates four static targets.
// These physical constraint endpoints contain no runtime interception code.
#pragma once
#include <array>
#include <cstddef>

#include "s3/core/physics/assembly.hpp"
#include "s3/core/physics/drive_frames.hpp"
#include "s3/core/physics/drive_parameters.hpp"
#include "s3/core/physics/rigid_body.hpp"
#include "s3/core/physics/skeleton_animation_record.hpp"

namespace s3::physics::skeleton_body::targets {

using assembly::BodySnapshot;
using drive_parameters::RetailDriveDynamics;
using drive_parameters::RetailDriveParams;
using physics::drive_frames::RetailDriveFrames;
using rigid_body::RetailSimulationStep;
using skeleton_animation_record::AnimationPartTransform;

inline constexpr std::size_t TARGET_COUNT = 4;
inline constexpr std::array<std::size_t, TARGET_COUNT> TARGET_PARTS{23, 0, 24, 25};
// pub(super)
inline constexpr float HARD_VELOCITY = rs::from_bits(0x4415FFFFu);
inline constexpr float HARD_STRENGTH = rs::from_bits(0x470C9FFFu);

struct SkeletonTargets {
    std::array<BodySnapshot, TARGET_COUNT> bodies{};
    std::array<RetailDriveFrames, TARGET_COUNT> frames{};
    std::array<RetailDriveDynamics, TARGET_COUNT> dynamics{};

    static SkeletonTargets new_(RetailSimulationStep simulation);
    /// Reset82BD9DB8/9DE0 normalizes these two desired matrices before
    /// PartSetTransform. The other two target parts retain their pose.
    void reset(AnimationPartTransform hips, AnimationPartTransform spawn);
    /// Native PartSetTransform preserves velocity and force state while
    /// changing the body transform. Its local mass frame is identity here.
    void set_transform(std::size_t index, AnimationPartTransform frame);
    AnimationPartTransform transform(std::size_t index) const;
    /// Ground::PredictFutureDeck82D38630 moves target0, not physical hips.
    void apply_future_deck_displacement(math::Vector3 displacement);

    // ---- target_update.rs ----
    ///82BE1618 updates only the two hook targets and returns the board-space
    /// transform consumed by the skeleton input owner.
    AnimationPartTransform update_hook_positions(const target_update::SkeletonTargetInput& input);
    target_update::SkeletonTargetUpdate update_positions(const target_update::SkeletonTargetInput& input,
                                                         SkeletonBody& skeleton);
    ///Complete82BE18B8, also called by Reset82BD9990 after constructing its
    ///COM frames. Replaces only physical extra-body velocities, never forces.
    target_update::ExtraTargetPositions update_extra_targets(SkeletonBody& skeleton,
                                                             const AnimationPartTransform& com_frame,
                                                             const AnimationPartTransform& lifted_com_frame);
};

// pub(super)
RetailDriveParams hard(float velocity, float strength);

}  // namespace s3::physics::skeleton_body::targets

namespace s3::physics::skeleton_body {
using targets::SkeletonTargets;
using targets::TARGET_COUNT;
}  // namespace s3::physics::skeleton_body
