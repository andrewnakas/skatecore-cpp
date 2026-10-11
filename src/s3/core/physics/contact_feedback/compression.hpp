// Port of skate-core/src/physics/contact_feedback/compression.rs.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/physics/board_pose.hpp"

namespace s3::physics::contact_feedback::compression {

using board_pose::PartPose;
using board_pose::PoseMatrix;

using V = std::array<float, 4>;
using M = std::array<V, 4>;

/// Complete 82585B58. Live bodies return their mass-frame basis/COM directly.
/// A body-less part with a local mass frame removes that frame from its cache.
/// This is distinct from 82585CB0's geometric-part transform getter.
PoseMatrix part_mass_transform(const PartPose& part);

/// Complete 82C08968: observe wheel COM positions in inverse deck mass space,
/// average native pairs0/1 and2/3, subtract board+8380, store+8372/+8376 only.
void calculate_wheel_compressions(std::array<std::uint8_t, 8400>& board, const std::array<PartPose, 7>& parts);

/// 82C08968, with live mass-frame observations rather than a byte-buffer owner.
/// Rest height is physicstrucks/default.TruckYPos, copied to8380 by82C08D18.
std::array<float, 2> average_wheel_compressions(M deck, std::array<V, 4> wheel_positions, float rest_height);

}  // namespace s3::physics::contact_feedback::compression

namespace s3::physics::contact_feedback {
using compression::average_wheel_compressions;
using compression::calculate_wheel_compressions;
using compression::part_mass_transform;
}  // namespace s3::physics::contact_feedback
