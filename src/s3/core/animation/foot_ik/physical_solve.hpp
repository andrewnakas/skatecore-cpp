// Port of skate-core/src/animation/foot_ik/physical_solve.rs.
// SkeletonIK::SolvePhysical82BF1480. Physical joint centers, not drive targets.
#pragma once
#include <array>
#include <cstddef>

#include "s3/core/animation/foot_ik/drive.hpp"
#include "s3/core/physics/skeleton_body.hpp"

namespace s3::animation::foot_ik::physical_solve {

/// pub(super) in Rust.
bool solve(physics::skeleton_body::SkeletonBody& body, const drive::Geometry& geometry, std::size_t end_part,
           std::array<float, 4> offset);

}  // namespace s3::animation::foot_ik::physical_solve
