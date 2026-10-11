// Port of skate-core/src/physics/board_world/broadphase.rs.
#include "s3/core/physics/board_world/broadphase.hpp"

namespace s3::physics::board_world {

using broadphase::conservative_bounds;
using broadphase::THIN_MARGIN;

std::vector<Range> BoardWorld::candidate_ranges(std::optional<Bounds> bounds) const {
    if (!(query_metadata_.has_value() && bounds.has_value() && bounds->valid())) {
        return {Range{0, triangles_.size()}};
    }
    const QueryMetadata& metadata = *query_metadata_;
    const Bounds b = *bounds;
    float scale = 1.0f;
    for (float v : {b.min.x, b.min.y, b.min.z, b.max.x, b.max.y, b.max.z}) scale = rs::max(scale, rs::abs(v));
    const Bounds expanded =
        b.expanded(maximum_fatness_ + maximum_triangle_margin_ + scale * (8.0f * rs::f32::EPSILON));
    std::vector<Range> out;
    for (std::size_t i : query_index_.query(expanded, metadata.meshes)) {
        out.push_back(metadata.meshes[i].triangle_range);
    }
    return out;
}

std::vector<std::pair<std::size_t, const WorldTriangle*>> BoardWorld::line_candidates(Vector3 start, Vector3 end,
                                                                                      float radius) const {
    const std::optional<Bounds> bounds = line_candidate_bounds(start, end, radius);
    std::vector<std::pair<std::size_t, const WorldTriangle*>> out;
    for (const Range& range : candidate_ranges(bounds)) {
        for (std::size_t i = range.start; i < range.end; ++i) {
            if (!query_metadata_.has_value() || !bounds.has_value() || triangle_bounds_[i].overlaps(*bounds)) {
                out.emplace_back(i, &triangles_[i]);
            }
        }
    }
    return out;
}

std::optional<Bounds> BoardWorld::line_candidate_bounds(Vector3 start, Vector3 end, float radius) const {
    if (!rs::is_finite(radius) || radius < 0.0f) {
        return std::nullopt;
    }
    const float span =
        rs::max(rs::max(rs::abs(end.x - start.x), rs::abs(end.y - start.y)), rs::abs(end.z - start.z));
    const std::optional<Bounds> b = Bounds::from_points({start, end});
    if (!b) return std::nullopt;
    return conservative_bounds(*b, radius + maximum_fatness_ + maximum_triangle_margin_ + span * THIN_MARGIN);
}

rs::Result<std::vector<std::size_t>, const char*> BoardWorld::candidate_mesh_indices(
    std::optional<Bounds> bounds) const {
    const auto metadata_result = query_metadata();
    if (metadata_result.is_err()) return rs::make_err(metadata_result.unwrap_err());
    const QueryMetadata& metadata = *metadata_result.unwrap();
    if (!(bounds.has_value() && bounds->valid())) {
        std::vector<std::size_t> all(metadata.meshes.size());
        for (std::size_t i = 0; i < all.size(); ++i) all[i] = i;
        return all;
    }
    return query_index_.query(conservative_bounds(*bounds, maximum_fatness_ + maximum_triangle_margin_),
                              metadata.meshes);
}

namespace broadphase {

std::optional<Bounds> primitive_bounds(const ContactPrimitive& primitive) {
    Vector3 center;
    float radius = 0.0f;
    if (const auto* s = std::get_if<Sphere>(&primitive.value)) {
        center = s->center;
        radius = s->radius;
    } else if (const auto* c = std::get_if<ContactPrimitive::Capsule>(&primitive.value)) {
        if (!rs::is_finite(c->radius)) {
            return std::nullopt;
        }
        const Vector3 offset{c->axis.x * c->half_length, c->axis.y * c->half_length, c->axis.z * c->half_length};
        const auto b = Bounds::from_points({
            Vector3{c->center.x - offset.x, c->center.y - offset.y, c->center.z - offset.z},
            Vector3{c->center.x + offset.x, c->center.y + offset.y, c->center.z + offset.z},
        });
        if (!b) return std::nullopt;
        return b->expanded(rs::abs(c->radius));
    } else if (const auto* r = std::get_if<ContactPrimitive::RoundedBox>(&primitive.value)) {
        if (!rs::is_finite(r->radius)) {
            return std::nullopt;
        }
        const float half[3] = {r->half_extents.x, r->half_extents.y, r->half_extents.z};
        float extent[3];
        for (std::size_t axis = 0; axis < 3; ++axis) {
            float sum = -0.0f;  // Iterator::sum::<f32> starts at -0.0
            for (std::size_t k = 0; k < 3; ++k) sum += rs::abs(half[k]) * rs::abs(r->basis.columns[k][axis]);
            extent[axis] = rs::abs(r->radius) + sum;
        }
        return Bounds::from_points({
            Vector3{r->center.x - extent[0], r->center.y - extent[1], r->center.z - extent[2]},
            Vector3{r->center.x + extent[0], r->center.y + extent[1], r->center.z + extent[2]},
        });
    } else if (const auto* t = std::get_if<Triangle>(&primitive.value)) {
        if (!rs::is_finite(t->fatness)) {
            return std::nullopt;
        }
        const auto b = Bounds::from_points(t->vertices);
        if (!b) return std::nullopt;
        return b->expanded(rs::abs(t->fatness));
    }
    if (!rs::is_finite(radius)) {
        return std::nullopt;
    }
    const auto b = Bounds::from_points({center});
    if (!b) return std::nullopt;
    return b->expanded(rs::abs(radius));
}

Bounds conservative_bounds(Bounds bounds, float padding) {
    float scale = 1.0f;
    for (float v : {bounds.min.x, bounds.min.y, bounds.min.z, bounds.max.x, bounds.max.y, bounds.max.z}) {
        scale = rs::max(scale, rs::abs(v));
    }
    return bounds.expanded(padding + scale * (8.0f * rs::f32::EPSILON));
}

}  // namespace broadphase
}  // namespace s3::physics::board_world
