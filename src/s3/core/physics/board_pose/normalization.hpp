// Port of skate-core/src/physics/board_pose/normalization.rs (module physics::board_pose::normalization).
#pragma once
#include "s3/core/physics/board_pose/arithmetic.hpp"

namespace s3::physics::board_pose::normalization {

/// 825C5710: z/y/x projection against ORIGINAL input axes, then normalization.
/// Translation is copied verbatim. Degenerate inputs receive no invented repair.
PoseMatrix orthonormalize_rotation(PoseMatrix input);

/// 82AE0A40: native magnitude gates and least-parallel pair selection followed
/// by two normalized cross products. This differs from 825C5710 above.
PoseMatrix orthonormalize_part_basis(PoseMatrix input);

}  // namespace s3::physics::board_pose::normalization
