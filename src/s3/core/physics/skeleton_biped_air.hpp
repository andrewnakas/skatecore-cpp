// Port of skate-core/src/physics/skeleton_biped_air.rs.
// Biped Skeleton82BDDD70: COM-constrained animation root, not a body teleport.
// Original S3 SHA431b8eba...0395a; ordinary PC arithmetic, not Xenon parity.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/core/physics/skeleton_root.hpp"

namespace s3::physics::skeleton_biped_air {

using Frame = skeleton_animation_record::AnimationPartTransform;

///82BDDDC8..DF54. The frame's translation is replaced by the trajectory COM
///minus the rotated LOCAL animation COM10960. Mapped board12624 is independent.
Frame prepare(skeleton_root::SkeletonRootFrames& roots, Frame frame, std::array<float, 4> trajectory_position,
              std::array<float, 4> local_com, const Frame& mapped_board, std::uint32_t flags_2476,
              std::uint32_t& flags_2468);

}  // namespace s3::physics::skeleton_biped_air
