// Port of skate-core/src/physics/skeleton_root.rs.
// Skeleton::UpdateRootTransforms82BE0318: actual board prediction and the
// retained animation-to-board/world frames. Full Reckoning is an input owner.
#pragma once
#include <array>
#include <optional>

#include "s3/core/physics/skeleton_animation_record.hpp"

namespace s3::physics::skeleton_root {

using skeleton_animation_record::AnimationPartTransform;

struct SkeletonRootFrames {
    // Default: Skeleton ctor82BD7260 matrix seeds,16416=0 and16417=1.
    AnimationPartTransform board = skeleton_animation_record::IDENTITY;
    AnimationPartTransform inverse_board = skeleton_animation_record::IDENTITY;
    std::array<float, 4> previous_board_position{};
    std::array<float, 4> predicted_board_position{};
    std::optional<std::array<float, 4>> supplied_prediction;
    AnimationPartTransform animation_to_board = skeleton_animation_record::IDENTITY;
    AnimationPartTransform animation_to_world = skeleton_animation_record::IDENTITY;
    AnimationPartTransform world_to_animation = skeleton_animation_record::IDENTITY;
    AnimationPartTransform heading_alignment = skeleton_animation_record::IDENTITY;
    bool initialize_heading = true;

    static SkeletonRootFrames default_() { return SkeletonRootFrames{}; }

    ///82BDFE58: teleport prediction uses the current physical deck position;
    ///the heading and remaining root composition match82BE0318.
    void update_teleport(AnimationPartTransform board, const AnimationPartTransform& animation_board,
                         const AnimationPartTransform& reckoning_frame_816);

    /// Reset82BD9990 writes these two frames from the initial mapped pose.
    /// Other root histories retain their independently initialized state.
    void reset_initial_alignment(AnimationPartTransform animation_to_world);

    void update(AnimationPartTransform board, std::array<float, 4> board_velocity, float time_step,
                const AnimationPartTransform& animation_board, const AnimationPartTransform& reckoning);
};

///825C5710: reverse-axis Gram-Schmidt, two rsqrt refinements, translation copy. (pub(crate))
AnimationPartTransform orthonormalize(AnimationPartTransform source);

/// Inline rigid inverse: transposeXYZ, zero stored W, translation Z,Y,X FMA. (pub(crate))
AnimationPartTransform inverse_rigid(const AnimationPartTransform& source);

}  // namespace s3::physics::skeleton_root
