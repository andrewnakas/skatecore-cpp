// Port of skate-core/src/physics/collision/world_contact.rs.
// Physics/world pair query 8277B720, called by triangle dispatch 8277BC58.
// This is distinct from the general Volume query (82AD3CD8).
#pragma once
#include <optional>

#include "s3/core/physics/collision/fixup.hpp"
#include "s3/core/physics/collision/sphere_triangle.hpp"

namespace s3::physics::collision::world_contact {

using fixup::ContactPair;
using math::Vector3;
using sphere_triangle::Sphere;
using sphere_triangle::Triangle;

struct WorldContactSettings {
    /// Per-volume padding at cached primitive +112, excluding its radius.
    float volume_padding = 0.0f;
    /// Query settings +0. Bounds the approaching velocity's prediction.
    float maximum_separating_distance = 0.0f;
    /// Query settings +4 and +8, passed directly to triangle fixup.
    float edge_cos_bend_normal_threshold = 0.0f;
    float convexity_epsilon = 0.0f;
    /// World-query context byte +61, forwarded to TU3 vertex handling.
    bool is_object = false;
};

struct WorldContact {
    /// B (world) toward A (moving primitive), ready for contact construction.
    Vector3 normal;
    ContactPair points;
};

/// 8277BF0C..BF8C. The original predicts using the literal 1/60, independent
/// of the simulation timestep. Do not replace it with a caller's frame dt.
float world_separation_limit(Vector3 velocity, Vector3 triangle_normal, float padding, float maximum);

/// Complete sphere branch of the physics/world pair query. Input GP geometry
/// is in world space; world broad phase, exclusion tables and duplicate-contact
/// buffering remain the producer's responsibilities.
std::optional<WorldContact> sphere_triangle_world_contact(Sphere sphere, Triangle triangle,
                                                          Vector3 velocity, WorldContactSettings settings);

}  // namespace s3::physics::collision::world_contact
