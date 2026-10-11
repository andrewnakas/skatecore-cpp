// Port of skate-game/src/physics/offboard/skeleton_ground/frames.rs
#pragma once
#include <cstdint>

#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/core/physics/skeleton_board_frames.hpp"
#include "s3/core/physics/skeleton_root.hpp"
#include "s3/game/physics/offboard/skeleton_ground.hpp"

namespace s3::game::physics::offboard::skeleton_ground::frames {

using sc::physics::skeleton_board_frames::SkeletonBoardFrames;
using sc::physics::skeleton_root::SkeletonRootFrames;

/// pub(super)
Transform prepare_pose(SkeletonRootFrames& roots, SkeletonBoardFrames& board_frames, const Transform& animation_board,
                       const Transform& mapped, Transform& retained, Input input, std::uint32_t flags_2476,
                       std::uint32_t flags_2484, std::uint32_t& flags_2468);
/// pub(super)
Transform prepare(SkeletonRootFrames& roots, const Transform& world, const Transform& mapped, Transform& retained,
                  std::uint32_t flags_2476, std::uint32_t flags_2484, std::uint32_t& flags_2468);

}  // namespace s3::game::physics::offboard::skeleton_ground::frames
