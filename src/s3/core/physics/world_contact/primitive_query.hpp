// Port of skate-core/src/physics/world_contact/primitive_query.rs.
// Physics/world primitive query 8277B720 and moving-volume/triangle dispatch
// 8277BC58. Geometry lives in typed host-owned records; only the recovered
// kernels' GP and feature scratch boundaries use native word layouts.
#pragma once
#include <optional>

#include "s3/core/physics/collision.hpp"
#include "s3/core/physics/world_contact/box_box_sat.hpp"
#include "s3/core/physics/world_contact/primitive_geometry.hpp"
#include "s3/core/physics/world_contact/triangle_box_sat.hpp"

namespace s3::physics::world_contact::primitive_query {

using collision::ContactPair;
using collision::Triangle;
using collision::WorldContactSettings;
using geometry::ContactPrimitive;
using geometry::transform_triangle_volume;
using geometry::triangle_from_volume;
using math::Vector3;
using projection::PrimitiveKind;

struct PrimitiveContactManifold {
    /// World primitive B toward moving primitive A, as expected by the solver.
    Vector3 normal;
    /// Native prism order is retained, including coincident points.
    std::array<ContactPair, 16> points{};
    std::size_t count = 0;
};

/// Full moving sphere/capsule/triangle/rounded-box query against a world
/// triangle. The caller retains native world-triangle then moving-volume order.
std::optional<PrimitiveContactManifold> primitive_triangle_world_contacts(const ContactPrimitive& primitive,
                                                                          Triangle triangle, Vector3 velocity,
                                                                          WorldContactSettings settings);

// pub(super)
MaximumFeature maximum(const std::array<std::uint32_t, 48>& gp, PrimitiveKind kind, std::uint32_t mode,
                       std::array<std::uint32_t, 4> direction);

}  // namespace s3::physics::world_contact::primitive_query
