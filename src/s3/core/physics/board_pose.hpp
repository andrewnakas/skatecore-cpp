// Port of skate-core/src/physics/board_pose.rs.
// Complete TU3 board SetTransform 82C0B2C8 and its part/pose dependencies.
// This changes poses only; it does not infer startup, settle joints or reset rates.
#pragma once
#include <array>

#include "s3/core/physics/board_pose/arithmetic.hpp"
#include "s3/core/physics/board_pose/normalization.hpp"
#include "s3/core/physics/board_pose/part.hpp"

namespace s3::physics::board_pose {

// `PoseMatrix` (= std::array<uint32_t, 16>) is declared in board_pose/arithmetic.hpp.
using normalization::orthonormalize_part_basis;
using normalization::orthonormalize_rotation;
using part::part_transform;
using part::PartPose;
using part::set_part_transform;

/// Set deck part 6 to the orthonormalized request, then move parts 0..5 by its
/// common rigid delta. The separate hook receives the ORIGINAL requested pose.
void set_board_transform(std::array<PartPose, 7>& parts, PartPose& hook, PoseMatrix requested);

}  // namespace s3::physics::board_pose
