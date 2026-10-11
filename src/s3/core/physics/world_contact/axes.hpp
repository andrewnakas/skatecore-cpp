// Port of skate-core/src/physics/world_contact/axes.rs.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace s3::physics::world_contact::axes {

/// Complete TU3 82ACEA30 candidate producer. GP records are the native 192-byte
/// representation: origin +0, normals +16/+32/+48, edges +64 onward, counts
/// bytes +140/+141. All four output lanes and speculative rejected stores are
/// preserved. Returns zero when the native function returns null.
std::size_t separating_axis_candidates(const std::array<std::uint32_t, 48>& a,
                                       const std::array<std::uint32_t, 48>& b,
                                       std::array<std::array<std::uint32_t, 4>, 16>& output);

}  // namespace s3::physics::world_contact::axes
