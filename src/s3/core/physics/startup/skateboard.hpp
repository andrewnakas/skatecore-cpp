// Port of skate-core/src/physics/startup/skateboard.rs.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/physics/board_pose.hpp"
#include "s3/core/physics/startup/reset.hpp"

namespace s3::physics::startup::skateboard {

using board_pose::PartPose;
using board_pose::PoseMatrix;

/// Complete 82C05F50 reset orchestration. `authored` supplies the seven matrices
/// produced by InitializeTransforms82C0ADF0 for the current stock geometry;
/// deck6 is published first, then wheels0..3 and trucks4..5. This explicit input
/// does not select a spawn or change joint anchors to a presumed settled pose.
///
/// `wrapper` is the native Skateboard prefix; `wrapper_base` only reconstructs
/// its three force-queue pointers. `body_state` is its SkateboardBody record.
/// Parts must have live bodies, as in the native seven-part board assembly.
/// Processed+2468 bit20 flips all four lanes of Ri and At before SetTransform.
/// The hook receives that target separately and its body rates are not reset.
std::uint32_t reset_skateboard(std::array<std::uint8_t, 1360>& wrapper, std::uint32_t wrapper_base,
                               std::array<std::uint8_t, 8400>& body_state, std::array<PartPose, 7>& parts,
                               PartPose& hook, std::array<PoseMatrix, 7> authored, PoseMatrix requested,
                               std::uint32_t processed_flags_2468,
                               const std::array<std::array<std::uint32_t, 4>, 7>& gravity);

}  // namespace s3::physics::startup::skateboard

namespace s3::physics::startup {
using skateboard::reset_skateboard;
}  // namespace s3::physics::startup
