// Port of skate-core/src/physics/rigid_body/dynamic_update.rs.
// RigidBody::DynamicUpdate82AE6590 reconstruction candidate.
// Guest pointers are opaque metadata; the caller supplies their referenced data.
#pragma once
#include <array>
#include <cstdint>
#include <utility>

#include "s3/core/physics/rigid_body.hpp"

namespace s3::physics::rigid_body::dynamic_update {

using V = std::array<float, 4>;

struct DynamicUpdateResult {
    std::array<float, 3> orientation_displacement{};
    /// Native cap-selected squared speeds used by the sleep metric.
    float linear_speed_squared = 0.0f;
    float angular_speed_squared = 0.0f;
};

/// Update every native body field and clear all 64 reaction bytes. `body` is the
/// 176-byte RigidBody prefix; `inertia` is its 40-byte Inertia record. Metadata
/// lanes, including pointers and IDs, are preserved. No activation is performed.
DynamicUpdateResult dynamic_update_packed(std::array<std::uint32_t, 44>& body,
                                          const std::array<std::uint32_t, 10>& inertia,
                                          RetailSimulationStep simulation,
                                          std::array<std::uint32_t, 16>& reactions);

// pub(super) helpers used by rigid_body.
V orientation(V q, V rotation);
std::array<V, 3> quaternion_basis(V q);
std::pair<V, V> inverse_inertia(std::array<V, 3> basis, std::array<std::uint32_t, 3> tensor);

}  // namespace s3::physics::rigid_body::dynamic_update

namespace s3::physics::rigid_body {
using dynamic_update::dynamic_update_packed;
using dynamic_update::DynamicUpdateResult;
}  // namespace s3::physics::rigid_body
