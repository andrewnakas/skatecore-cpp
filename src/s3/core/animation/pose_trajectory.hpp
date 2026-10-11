// Port of skate-core/src/animation/pose_trajectory.rs.
// TU3 FetchSys trajectory delta82D178D8. This modifies only the trajectory SQT.
#pragma once
#include <array>
#include <optional>

#include "s3/core/animation/output.hpp"

namespace s3::animation::pose_trajectory {

using output::Sqt;

struct LoopTransform {
    std::array<float, 4> rotation{};
    std::array<float, 3> translation{};
};
S3_DERIVE_EQ(LoopTransform, rotation, translation)

/// BatchFetch82D20A40 selects the authored transform when the loop argument is
/// nonzero, otherwise identity. It does not exponentiate by the loop count.
Sqt delta(Sqt current, Sqt previous, std::optional<LoopTransform> loop_transform);

/// pub(super) in Rust.
std::array<float, 3> rotate(std::array<float, 4> q, std::array<float, 3> v);
/// pub(super) in Rust.
std::array<float, 4> multiply(std::array<float, 4> a, std::array<float, 4> b);

}  // namespace s3::animation::pose_trajectory
