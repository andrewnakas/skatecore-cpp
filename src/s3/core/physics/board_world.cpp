// Port of skate-core/src/physics/board_world.rs.
#include "s3/core/physics/board_world.hpp"

#include "s3/core/physics/board_runtime.hpp"

namespace s3::physics::board_world {

using broadphase::conservative_bounds;
using broadphase::primitive_bounds;

namespace {
BoardCollision collision_from_record(const ContactRecord& record) {
    auto f = [&](std::size_t i) { return rs::from_bits(record[i]); };
    auto vector = [&](std::size_t i) { return Vector3{f(i), f(i + 1), f(i + 2)}; };
    return BoardCollision{
        CollisionBody::from_contact_id(record[3]),
        CollisionBody::StaticWorld(),
        RetailContactInput{vector(0), vector(4), vector(8), f(11), f(15), f(19), record[23]},
    };
}
}  // namespace

std::optional<WorldTriangle> WorldTriangle::from_vertices(std::array<Vector3, 3> vertices,
                                                          RetailContactMaterial material, std::uint32_t tag,
                                                          std::uint32_t flags, std::array<float, 3> edge_cosines,
                                                          float fatness) {
    for (const Vector3& v : vertices) {
        if (!rs::is_finite(v.x) || !rs::is_finite(v.y) || !rs::is_finite(v.z)) {
            return std::nullopt;
        }
    }
    const Triangle triangle = world_contact::triangle_from_volume(vertices, fatness, edge_cosines, flags);
    const Vector3 normal = triangle.feature.normal;
    bool bad_edge = false;
    for (float v : triangle.edge_lengths) bad_edge = bad_edge || !rs::is_finite(v) || v <= 0.0f;
    if (bad_edge || !rs::is_finite(normal.x) || !rs::is_finite(normal.y) || !rs::is_finite(normal.z) ||
        normal == Vector3::ZERO) {
        return std::nullopt;
    }
    return WorldTriangle{triangle, material, tag};
}

BoardWorld BoardWorld::new_(std::vector<WorldTriangle> triangles) {
    BoardWorld world;
    world.triangle_bounds_.reserve(triangles.size());
    for (const WorldTriangle& t : triangles) {
        const std::optional<Bounds> b = Bounds::from_points(t.triangle.vertices);
        world.triangle_bounds_.push_back(
            b ? *b
              : Bounds{
                    Vector3{rs::f32::NEG_INFINITY, rs::f32::NEG_INFINITY, rs::f32::NEG_INFINITY},
                    Vector3{rs::f32::INFINITY_, rs::f32::INFINITY_, rs::f32::INFINITY_},
                });
    }
    float maximum_fatness = 0.0f;
    for (const WorldTriangle& t : triangles) {
        const float v = rs::is_finite(t.triangle.fatness) && t.triangle.fatness >= 0.0f ? t.triangle.fatness
                                                                                          : rs::f32::INFINITY_;
        maximum_fatness = rs::max(maximum_fatness, v);
    }
    // The current thin-triangle leaf accepts barycentric weights down to
    // -epsilon. Two negative weights can extend an axis by 2*epsilon*span.
    float maximum_triangle_margin = 0.0f;
    for (const Bounds& b : world.triangle_bounds_) {
        const float span = rs::max(rs::max(b.max.x - b.min.x, b.max.y - b.min.y), b.max.z - b.min.z);
        maximum_triangle_margin = rs::max(maximum_triangle_margin, span * (2.0f * broadphase::THIN_MARGIN));
    }
    world.triangles_ = std::move(triangles);
    world.query_metadata_ = std::nullopt;
    world.query_index_ = query_index::QueryIndex{};
    world.maximum_fatness_ = maximum_fatness;
    world.maximum_triangle_margin_ = maximum_triangle_margin;
    world.contacts_.clear();
    world.buffer_ = ContactBuffer{};
    world.buffer_.count = 0;
    world.buffer_.flushed = 0;
    world.buffer_.capacity = 0;
    world.buffer_.dropped = 0;
    world.buffer_.distance_squared_threshold = 0.0f;
    world.buffer_.allow_flush = 1;
    world.buffer_.deferred_reduction = 0;
    world.buffer_.full = 0;
    return world;
}

rs::Result<BoardWorld, const char*> BoardWorld::with_query_metadata(std::vector<WorldTriangle> triangles,
                                                                    QueryMetadata metadata) {
    const auto valid = metadata.validate(triangles);
    if (valid.is_err()) return rs::make_err(valid.unwrap_err());
    BoardWorld world = new_(std::move(triangles));
    world.query_index_ = query_index::QueryIndex(metadata.meshes);
    world.query_metadata_ = std::move(metadata);
    return world;
}

rs::Result<const QueryMetadata*, const char*> BoardWorld::query_metadata() const {
    if (!query_metadata_) return rs::make_err("Canonical world has no authored query metadata");
    return &*query_metadata_;
}

rs::Result<std::optional<WorldLineHit>, const char*> BoardWorld::query_thin_line(Vector3 start, Vector3 end) const {
    return query_swept_line(start, end, 0.0f);
}

rs::Result<std::optional<WorldLineHit>, const char*> BoardWorld::query_swept_line(Vector3 start, Vector3 end,
                                                                                  float radius) const {
    if (!rs::is_finite(radius) || radius < 0.0f) {
        return rs::make_err("line query radius must be finite and nonnegative");
    }
    const Vector3 direction{end.x - start.x, end.y - start.y, end.z - start.z};
    std::optional<WorldLineHit> nearest;
    for (const auto& [index, entry] : line_candidates(start, end, radius)) {
        (void)index;
        TriangleLineHit geometry{Vector3::ZERO, Vector3::ZERO, 0.0f, {0.0f, 0.0f, 0.0f}};
        if (triangle_query::triangle_segment(geometry, start, direction, entry->triangle.vertices, radius,
                                             entry->triangle.fatness)) {
            if (!nearest || geometry.fraction < nearest->geometry.fraction) {
                nearest = WorldLineHit{geometry, entry->tag};
            }
        }
    }
    return nearest;
}

Slice<const BoardCollision> BoardWorld::query(const board_runtime::BoardRuntime& board, WheelWorldSettings settings) {
    const auto poses = board.part_transforms();
    std::vector<BoardWorldVolume> volumes;
    for (std::size_t k = 0; k < 4; ++k) {
        const BodyId id = board::BodyId_ORDER[k];
        const auto& body = board.bodies()[board::index(id)];
        if (body.state_flags != 1) {
            volumes.push_back(BoardWorldVolume{
                CollisionBody::Board(id),
                Sphere{poses[board::index(id)].translation, settings.radius},
                body.rates.linear_velocity,
                settings.material,
            });
        }
    }
    return query_primitives(volumes, settings.query, settings.retention);
}

Slice<const BoardCollision> BoardWorld::query_primitives(Slice<const BoardWorldVolume> volumes,
                                                         WorldContactSettings query,
                                                         ContactRetentionSettings retention) {
    contacts_.clear();
    buffer_.count = 0;
    buffer_.flushed = 0;
    buffer_.dropped = 0;
    buffer_.full = 0;
    buffer_.capacity = retention.capacity;
    buffer_.distance_squared_threshold = retention.duplicate_distance_squared;
    buffer_.deferred_reduction = static_cast<std::uint8_t>(retention.deferred_reduction);
    const float padding = rs::is_finite(query.volume_padding) && rs::is_finite(query.maximum_separating_distance)
                              ? rs::max(query.volume_padding, 0.0f) + rs::max(query.maximum_separating_distance, 0.0f)
                              : rs::f32::INFINITY_;
    std::vector<std::optional<Bounds>> volume_bounds;
    volume_bounds.reserve(volumes.size());
    for (const BoardWorldVolume& v : volumes) {
        const std::optional<Bounds> b = primitive_bounds(v.primitive);
        volume_bounds.push_back(b ? std::optional<Bounds>(conservative_bounds(*b, padding + maximum_fatness_))
                                  : std::nullopt);
    }
    std::optional<Bounds> bounds;
    {
        bool all = true;
        std::vector<Vector3> points;
        for (const auto& b : volume_bounds) {
            if (!b) {
                all = false;
                break;
            }
            points.push_back(b->min);
            points.push_back(b->max);
        }
        if (all) {
            const std::optional<Bounds> joined = Bounds::from_points(points);
            if (joined) bounds = joined->expanded(padding);
        }
    }
    const std::vector<Range> ranges = candidate_ranges(bounds);
    std::vector<BoardCollision>& output = contacts_;
    const world_contact::buffer::ContactSink publish = [&output](const ContactRecord* records, std::size_t count) {
        for (std::size_t i = 0; i < count; ++i) output.push_back(collision_from_record(records[i]));
    };
    for (const Range& range : ranges) {
        for (std::size_t index = range.start; index < range.end; ++index) {
            const WorldTriangle& entry = triangles_[index];
            for (std::size_t v = 0; v < volumes.size(); ++v) {
                const BoardWorldVolume& volume = volumes[v];
                const std::optional<Bounds>& vb = volume_bounds[v];
                if (query_metadata_.has_value() && vb.has_value() && !triangle_bounds_[index].overlaps(*vb)) {
                    continue;
                }
                const auto manifold = world_contact::primitive_triangle_world_contacts(
                    volume.primitive, entry.triangle, volume.linear_velocity, query);
                if (!manifold) {
                    continue;
                }
                const RetailContactMaterial material = contact::combine_contact_materials(volume.material, entry.material);
                for (std::size_t p = 0; p < manifold->count; ++p) {
                    const auto& pair = manifold->points[p];
                    const RetailContactInput contact{
                        pair.a,
                        pair.b,
                        manifold->normal,
                        material.restitution,
                        material.static_friction,
                        material.dynamic_friction,
                        entry.tag,
                    };
                    const std::optional<std::size_t> slot = buffer_.allocate(publish);
                    if (!slot) {
                        buffer_.flush(publish);
                        return contacts();
                    }
                    buffer_.records[*slot] = retention_record(volume.body, contact);
                    // 8277C23C removes the most recent record on rejection.
                    if (buffer_.last_is_duplicate()) {
                        buffer_.count -= 1;
                    }
                }
            }
        }
    }
    buffer_.flush(publish);
    return contacts();
}

ContactRecord retention_record(CollisionBody id, RetailContactInput contact) {
    ContactRecord record{};
    const std::pair<std::size_t, Vector3> vectors[3] = {
        {0, contact.position_on_a},
        {4, contact.position_on_b},
        {8, contact.normal},
    };
    for (const auto& [offset, v] : vectors) {
        record[offset] = rs::to_bits(v.x);
        record[offset + 1] = rs::to_bits(v.y);
        record[offset + 2] = rs::to_bits(v.z);
    }
    record[3] = id.contact_id();
    record[7] = 0xFFFFFFFFu;
    record[11] = rs::to_bits(contact.restitution);
    record[15] = rs::to_bits(contact.static_friction);
    record[19] = rs::to_bits(contact.dynamic_friction);
    record[23] = contact.tag;
    return record;
}

}  // namespace s3::physics::board_world
