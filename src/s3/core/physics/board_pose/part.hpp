// Port of skate-core/src/physics/board_pose/part.rs (module physics::board_pose::part).
#pragma once
#include <array>
#include <cstdint>
#include <optional>

#include "s3/core/physics/board_pose/arithmetic.hpp"

namespace s3::physics::board_pose::part {

/// Explicit referenced records for a native 96-byte Part. Optional fields model
/// the actual null-pointer branches; callers own loading and activation policy.
struct PartPose {
    PoseMatrix transform{};
    /// Record addressed by Part+68 (the local mass-frame transform).
    std::optional<PoseMatrix> local_mass_frame;
    std::optional<std::array<std::uint32_t, 44>> body;
    std::optional<std::array<std::uint32_t, 10>> inertia;
};

/// 82585CB0: return the Part transform from its body, or its cached part matrix
/// if no body exists. This is not the whole game Body::GetPosition method.
PoseMatrix part_transform(const PartPose& part);

/// 82BD4318: update a live body's pose/caches, then always copy the original
/// requested matrix to the part. Velocity, forces and other packed lanes survive.
void set_part_transform(PartPose& part, PoseMatrix requested);

}  // namespace s3::physics::board_pose::part
