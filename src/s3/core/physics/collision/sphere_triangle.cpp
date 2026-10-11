// Port of skate-core/src/physics/collision/sphere_triangle.rs.
#include "s3/core/physics/collision/sphere_triangle.hpp"

#include <utility>

#include "s3/core/physics/collision/arithmetic.hpp"

namespace s3::physics::collision::sphere_triangle {

using namespace arithmetic;
using fixup::fix_up_triangle;
using fixup::TriangleFixup;

namespace {

float min_(float a, float b) { return a < b ? a : b; }
float max_(float a, float b) { return a > b ? a : b; }

std::pair<float, float> interval_gaps(Sphere sphere, Triangle triangle, Vector3 direction,
                                      bool triangle_is_a) {
    // 82ADD800 / 82ADE3B8. GP intervals exclude primitive fatness.
    const float s = dot(direction, sphere.center);
    const float p[3] = {dot(direction, triangle.vertices[0]), dot(direction, triangle.vertices[1]),
                        dot(direction, triangle.vertices[2])};
    const float lo = min_(min_(p[0], p[1]), p[2]);
    const float hi = max_(max_(p[0], p[1]), p[2]);
    if (triangle_is_a) {
        return {lo - s, s - hi};
    }
    return {s - hi, lo - s};
}

std::pair<Vector3, std::uint32_t> point_face(Vector3 point, Triangle t, Vector3 axis, bool triangle_is_a) {
    // The sphere contributes no axes or edges (82ACEA30), so the selected
    // direction is the triangle face normal. Thus GetMaximumFeature's face
    // branch is the complete reachable branch for this primitive pair.
    const Vector3 query = triangle_is_a ? axis : neg(axis);
    const bool same = (dot(query, t.feature.normal) < 0.0f) == triangle_is_a;
    const std::array<std::size_t, 3> indices = same ? std::array<std::size_t, 3>{0, 1, 2}
                                                    : std::array<std::size_t, 3>{2, 1, 0};
    const std::array<std::size_t, 3> vertices = same ? std::array<std::size_t, 3>{0, 2, 1}
                                                     : std::array<std::size_t, 3>{0, 1, 2};
    std::uint32_t region = same ? 8 : 0;
    Vector3 projected = madd(axis, dot(axis, sub(t.vertices[0], point)), point);
    std::size_t best = 0;
    float violation = 0.0f;
    std::array<Vector3, 3> planes;
    for (std::size_t k = 0; k < 3; ++k) {
        const std::size_t i = indices[k];
        const Vector3 direction = same ? t.feature.edges[i] : neg(t.feature.edges[i]);
        const Vector3 plane = cross(direction, axis);
        const float squared = dot(plane, plane);
        // Feature::BuildEdgePlanes (82AC6F88): one refinement and select-zero.
        // CRT initializer 82F837D8 copies 8212BF78 into 830BDD40. The mapped
        // image alone contains zero here; live TU3 uses 0x34000000.
        planes[k] = scale(plane, squared > rs::from_bits(0x34000000u) ? inverse_length_squared(squared, 1)
                                                                      : 0.0f);
    }
    for (std::size_t i = 0; i < 3; ++i) {
        const float d = dot(planes[i], sub(projected, t.vertices[vertices[i]]));
        if (i == 0 || d > violation) {
            best = i;
            violation = d;
        }
    }
    if (violation > 0.0f) {
        projected = sub(projected, scale(planes[best], violation));
        const std::size_t i = indices[best];
        const Vector3 base = t.vertices[vertices[best]];
        const Vector3 direction = same ? t.feature.edges[i] : neg(t.feature.edges[i]);
        const float along = dot(sub(projected, base), direction);
        Vector3 p;
        std::uint32_t r;
        if (along < 0.0f) {
            p = base;
            r = 1;
        } else if (along > t.edge_lengths[i]) {
            p = madd(direction, t.edge_lengths[i], base);
            r = 3;
        } else {
            p = madd(direction, along, base);
            r = 2;
        }
        projected = p;
        region += static_cast<std::uint32_t>(best) * 2 + r;
    }
    return {projected, region};
}

}  // namespace

std::optional<SphereTrianglePair> intersect_sphere_triangle(Sphere sphere, Triangle triangle,
                                                            float minimum_separating_distance,
                                                            bool triangle_is_a) {
    const auto prism = build_prism(sphere, triangle, minimum_separating_distance, triangle_is_a, false);
    if (!prism) return std::nullopt;
    const Vector3 separating_direction = prism->separating_direction;
    const float separating_distance = prism->separating_distance;
    Vector3 normal = prism->normal;
    ContactPair raw_points = prism->raw_points;
    const std::uint32_t triangle_region = prism->triangle_region;
    const float fat_a = triangle_is_a ? triangle.fatness : sphere.radius;
    const float fat_b = triangle_is_a ? sphere.radius : triangle.fatness;
    const ContactPair points{
        madd(normal, fat_a, raw_points.a),
        sub(raw_points.b, scale(normal, fat_b)),
    };
    const float distance = dot(sub(points.b, points.a), normal);
    // 82AD40C0..4308 publishes points/fatness/distances BEFORE fixup. The
    // fixup's reprojection edits its local prism; TU3 does not recopy those
    // points into this result. Only the result normal is shared with fixup.
    const TriangleFixup settings{!triangle_is_a, -1.0f, 0.0f, false};
    ContactPair local[1] = {raw_points};
    if (!fix_up_triangle(triangle.feature, normal, local, 1, settings)) {
        return std::nullopt;
    }
    return SphereTrianglePair{separating_direction, separating_distance, normal, points, distance,
                              triangle_region};
}

std::optional<SphereTrianglePrism> build_prism(Sphere sphere, Triangle triangle,
                                               float minimum_separating_distance, bool triangle_is_a,
                                               bool contact_query) {
    const Vector3 axis = triangle.feature.normal;
    const auto [a, b] = interval_gaps(sphere, triangle, axis, triangle_is_a);
    // 82ACF070 chooses the second gap on ties.
    const Vector3 separating_direction = a > b ? neg(axis) : axis;
    const float separating_distance = a > b ? a : b;
    const float fat_a = triangle_is_a ? triangle.fatness : sphere.radius;
    const float fat_b = triangle_is_a ? sphere.radius : triangle.fatness;
    if (separating_distance > (fat_b + minimum_separating_distance) + fat_a) {
        return std::nullopt;
    }
    const auto [face_point, triangle_region] =
        point_face(sphere.center, triangle, separating_direction, triangle_is_a);
    const ContactPair raw_points = triangle_is_a ? ContactPair{face_point, sphere.center}
                                                 : ContactPair{sphere.center, face_point};
    const Vector3 delta = sub(raw_points.b, raw_points.a);
    const float squared = dot(delta, delta);
    const float inverse_length = inverse_length_squared(squared, 2);
    // 8277B830..B8A8 tests the refined LENGTH, whereas 82AD3CD8 tests
    // squared length. Both compare against the literal 821408EC.
    const float gate = contact_query ? (squared == 0.0f ? 0.0f : squared * inverse_length) : squared;
    Vector3 normal = separating_direction;
    if (gate > rs::from_bits(0x34000000u)) {
        const Vector3 candidate = scale(delta, inverse_length);
        const auto [ca, cb] = interval_gaps(sphere, triangle, candidate, triangle_is_a);
        const float maximum = ca > cb ? ca : cb;
        if (separating_distance <= maximum) {
            normal = ca > cb ? neg(candidate) : candidate;
        }
    }
    return SphereTrianglePrism{separating_direction, separating_distance, normal, raw_points,
                               triangle_region};
}

}  // namespace s3::physics::collision::sphere_triangle
