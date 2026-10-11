// Port of skate-core/src/physics/board_world.rs.
// Contacts for every board primitive against the host triangle world.
// Rust owns geometry storage and traversal. Recovered query8277B720,
// triangle fixup82AD3130, material combine82763078 and contact retention
// determine the physical result, in world-triangle then moving-volume order.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "s3/core/math.hpp"
#include "s3/core/physics/board.hpp"
#include "s3/core/physics/board_step.hpp"
#include "s3/core/physics/board_world/query_index.hpp"
#include "s3/core/physics/board_world/query_metadata.hpp"
#include "s3/core/physics/collision.hpp"
#include "s3/core/physics/contact.hpp"
#include "s3/core/physics/slice.hpp"
#include "s3/core/physics/triangle_query.hpp"
#include "s3/core/physics/world_contact.hpp"

namespace s3::physics::board_runtime {
class BoardRuntime;
}

namespace s3::physics::board_world {

using board::BodyId;
using board_step::BoardCollision;
using board_step::CollisionBody;
using collision::Sphere;
using collision::Triangle;
using collision::WorldContactSettings;
using contact::RetailContactInput;
using contact::RetailContactMaterial;
using math::Vector3;
using query_metadata::Bounds;
using query_metadata::QueryMetadata;
using query_metadata::Range;
using triangle_query::TriangleLineHit;
using world_contact::ContactBuffer;
using world_contact::ContactPrimitive;
using world_contact::ContactRecord;

struct WorldTriangle {
    Triangle triangle;
    RetailContactMaterial material;
    std::uint32_t tag = 0;

    /// Host geometry preparation, with explicit adjacency and sidedness.
    /// Uses the native Volume constructor and its authored winding. Invalid
    /// host geometry is rejected without substituting a repaired triangle.
    static std::optional<WorldTriangle> from_vertices(std::array<Vector3, 3> vertices, RetailContactMaterial material,
                                                      std::uint32_t tag, std::uint32_t flags,
                                                      std::array<float, 3> edge_cosines, float fatness);
};

struct WorldLineHit {
    TriangleLineHit geometry;
    std::uint32_t tag = 0;
};
S3_DERIVE_EQ(WorldLineHit, geometry, tag)

/// Caller-resolved TempContactBuffer settings. Retention changes solver rows,
/// so its thresholds/capacity remain explicit instead of using tuned defaults.
struct ContactRetentionSettings {
    std::uint32_t capacity = 0;
    float duplicate_distance_squared = 0.0f;
    bool deferred_reduction = false;
};

struct WheelWorldSettings {
    /// Sphere shared by wheel definitions in82C0AA78.
    float radius = 0.0f;
    RetailContactMaterial material;
    WorldContactSettings query;
    ContactRetentionSettings retention;
};

/// One enabled moving primitive, with its owning solver body and material.
struct BoardWorldVolume {
    CollisionBody body;
    ContactPrimitive primitive = Sphere{};
    Vector3 linear_velocity;
    RetailContactMaterial material;
};

/// World geometry remains in supplied order. Each query reuses output storage;
/// the returned contacts are valid until the next mutable call.
class BoardWorld {
public:
    static BoardWorld new_(std::vector<WorldTriangle> triangles);

    static rs::Result<BoardWorld, const char*> with_query_metadata(std::vector<WorldTriangle> triangles,
                                                                   QueryMetadata metadata);

    rs::Result<const QueryMetadata*, const char*> query_metadata() const;

    const std::vector<WorldTriangle>& triangles() const { return triangles_; }

    /// Board wheel segments use zero query radius; rounded world triangles
    /// still select the native swept branch through their own fatness.
    rs::Result<std::optional<WorldLineHit>, const char*> query_thin_line(Vector3 start, Vector3 end) const;

    /// Ordered nearest-hit traversal for wheel and camera queries. The host
    /// owns traversal; each candidate uses the complete TU3 triangle dispatcher.
    rs::Result<std::optional<WorldLineHit>, const char*> query_swept_line(Vector3 start, Vector3 end,
                                                                          float radius) const;

    /// Sphere-only convenience for callers querying wheels. The game uses
    /// query_primitives with all enabled deck/truck/wheel children.
    Slice<const BoardCollision> query(const board_runtime::BoardRuntime& board, WheelWorldSettings settings);

    /// World-space shapes come from the live part poses and authored children.
    /// Cluster bounds include shape radii and the maximum predictive padding.
    /// Candidate pairs retain world-triangle then moving-volume order.
    Slice<const BoardCollision> query_primitives(Slice<const BoardWorldVolume> volumes, WorldContactSettings query,
                                                 ContactRetentionSettings retention);

    Slice<const BoardCollision> contacts() const {
        return Slice<const BoardCollision>(contacts_.data(), contacts_.size());
    }

    std::uint32_t dropped_contacts() const { return buffer_.dropped; }

    // ---- board_world/broadphase.rs (`impl BoardWorld`) ----

    /// Conservative cluster culling, retaining canonical traversal order and
    /// every triangle in intersecting clusters. Narrow-phase remains unchanged.
    std::vector<Range> candidate_ranges(std::optional<Bounds> bounds) const;

    /// Rust returns an iterator of `(usize, &WorldTriangle)`.
    std::vector<std::pair<std::size_t, const WorldTriangle*>> line_candidates(Vector3 start, Vector3 end,
                                                                             float radius) const;

    /// Bounds shared with adapters that retain mesh pool/group/identity filtering.
    /// Includes the current thin-leaf endpoint and barycentric margins.
    std::optional<Bounds> line_candidate_bounds(Vector3 start, Vector3 end, float radius) const;

    /// ZIP hierarchy results in canonical mesh order, for current metadata consumers.
    /// Pool, matching group, rejection mask and identity stay caller-owned.
    rs::Result<std::vector<std::size_t>, const char*> candidate_mesh_indices(std::optional<Bounds> bounds) const;

private:
    BoardWorld() = default;
    std::vector<WorldTriangle> triangles_;
    std::vector<Bounds> triangle_bounds_;
    std::optional<QueryMetadata> query_metadata_;
    query_index::QueryIndex query_index_;
    float maximum_fatness_ = 0.0f;
    float maximum_triangle_margin_ = 0.0f;
    std::vector<BoardCollision> contacts_;
    ContactBuffer buffer_{};
};

/// Rust-private, exposed for the ported tests.
/// Retention reads only contact header geometry, body IDs and material/tag.
/// BoardStep builds the actual body workspaces after this frame's force queue
/// has been applied, preventing stale acceleration copies in solver contacts.
ContactRecord retention_record(CollisionBody id, RetailContactInput contact);

}  // namespace s3::physics::board_world

#include "s3/core/physics/board_world/broadphase.hpp"
