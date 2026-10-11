// Port of skate-core/src/physics/skeleton_air_frames.rs.
// Original TU3 animated-board and COM-controlled air frame calculations.
// 82BDDA10, 82BDE600 and 82BDE7B8. Persistent frames remain in their owners.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/core/physics/skeleton_board_frames.hpp"
#include "s3/core/physics/skeleton_root.hpp"

namespace s3::physics::skeleton_air_frames {

using Transform = skeleton_animation_record::AnimationPartTransform;
using skeleton_board_frames::SkeletonBoardFrames;
using skeleton_root::SkeletonRootFrames;

/// 82B985E8 publishes the one-frame request and retained frame count;
/// 82BDE920 loads that count with lwz before unsigned-to-floating conversion.
struct AirDismountRevert {
    bool requested = false;
    std::uint32_t frames = 0;
    bool goofy = false;
};

/// 82BDDA5C..DBBC, after the common UpdateRootTransforms82BE0318.
/// The skate root uses the unblended animation target in this path.
Transform prepare_animated(const SkeletonRootFrames& roots, SkeletonBoardFrames& board, const Transform& mapped_board,
                           std::uint32_t& flags);

/// 82BDE7B8: preserve prior heading on entry, apply an authored dismount
/// revert, then place the current animation COM at the integrated air target.
/// It does not update animation_to_board/inverse_board or consume prediction.
void update_known_air_roots(SkeletonRootFrames& roots, const Transform& reckoning, std::array<float, 4> target_com,
                            std::array<float, 4> animation_com, AirDismountRevert revert);

///82BDE634..6DC. ApplyBoardAnimation follows before the completion below.
Transform prepare_known_air(const SkeletonRootFrames& roots, SkeletonBoardFrames& board,
                            const Transform& mapped_board, std::uint32_t& flags);

///82BDE748..7A4, after applying the target and the board velocity update.
void finish_known_air(SkeletonRootFrames& roots, SkeletonBoardFrames& board, Transform effective_board);

///82BDEEB8/82BE0E80: anchor an animation bone or COM in world space.
///Plants retain heading_alignment and do not consume board prediction.
void update_plant_roots(SkeletonRootFrames& roots, const Transform& reckoning, std::array<float, 4> world_anchor,
                        std::array<float, 4> animation_anchor);

}  // namespace s3::physics::skeleton_air_frames
