// Port of skate-core/src/physics/skeleton_body/target_update.rs (included via skeleton_body.hpp).
// Original Skeleton::UpdateHookPositions82BE1618 and extra followers82BE18B8.
// Their target changes precede the returned teleport/large-displacement gate.
//
// The Rust `impl SkeletonTargets` methods of this file are declared in targets.hpp
// and defined in target_update.cpp.
#pragma once
#include <array>

#include "s3/core/physics/skeleton_animation_record.hpp"

namespace s3::physics::skeleton_body::target_update {

using Transform = skeleton_animation_record::AnimationPartTransform;

/// (Rust borrows `'a` references; held here as const references.)
struct SkeletonTargetInput {
    ///Skeleton11792, mapped physical animation hips, not physical COM.
    const Transform& animation_hips;
    ///Skeleton11728, mapped physical animation board.
    const Transform& animation_board;
    ///Skeleton11920.
    const Transform& animation_to_world;
    ///Skeleton12368, inverse of the actual physical board.
    const Transform& inverse_board;
    ///Skeleton15760, published by the current movement branch.
    const Transform& skate_root;
    ///Skeleton15824 and15888 from the actual COM/lift producer.
    const Transform& com_frame;
    const Transform& lifted_com_frame;
    ///Skeleton16508.
    bool teleporting;
};

struct ExtraTargetPositions {
    ///Skeleton11632,11648,11664 respectively.
    std::array<float, 4> com{};
    std::array<float, 4> lifted_com{};
    std::array<float, 4> following_com{};
};

struct SkeletonTargetUpdate {
    ///Skeleton12240, inverse physical board * animated board in world space.
    Transform animation_board_to_physics{};
    ExtraTargetPositions positions;
    ///False triggers GeneralUpdate's real teleport/reset behavior after all
    ///target and velocity writes, not a guard suppressing those writes.
    bool continuous = false;
};

}  // namespace s3::physics::skeleton_body::target_update

namespace s3::physics::skeleton_body {
using target_update::ExtraTargetPositions;
using target_update::SkeletonTargetInput;
using target_update::SkeletonTargetUpdate;
}  // namespace s3::physics::skeleton_body
