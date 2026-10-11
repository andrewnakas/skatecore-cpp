// Port of skate-core/src/physics/world_contact/primitive_geometry.rs.
#include "s3/core/physics/world_contact/primitive_geometry.hpp"

#include "s3/core/physics/reciprocal_sqrt.hpp"
#include "s3/core/physics/world_contact/prism_math.hpp"

namespace s3::physics::world_contact::primitive_query::geometry {

using namespace prism_math;
using reciprocal_sqrt::estimate;

namespace {
float refined_inverse(float squared) {
    const float seed = estimate(squared);
    return rs::mul_add(seed * 0.5f, rs::mul_add(-squared, seed * seed, 1.0f), seed);
}
void put(std::uint32_t* out, std::size_t offset, V v) { store_bits(out, offset, v); }
}  // namespace

Triangle triangle_from_volume(std::array<Vector3, 3> vertices, float fatness, std::array<float, 3> edge_cosines,
                              std::uint32_t volume_flags) {
    std::array<V, 3> points;
    for (std::size_t i = 0; i < 3; ++i) points[i] = lanes(vertices[i], 1.0f);
    const V raw_normal = cross(sub(points[1], points[0]), sub(points[2], points[0]));
    const float squared = dot(raw_normal, raw_normal);
    const V normal = squared > rs::from_bits(0x34000000u) ? scale(raw_normal, refined_inverse(squared)) : raw_normal;
    const std::array<V, 3> raw_edges = {
        sub(points[2], points[0]),
        sub(points[1], points[2]),
        sub(points[0], points[1]),
    };
    std::array<float, 3> inverse;
    for (std::size_t i = 0; i < 3; ++i) inverse[i] = refined_inverse(dot(raw_edges[i], raw_edges[i]));
    Triangle t;
    t.vertices = vertices;
    t.feature.normal = xyz(normal);
    for (std::size_t i = 0; i < 3; ++i) t.feature.edges[i] = xyz(scale(raw_edges[i], inverse[i]));
    t.feature.flags = volume_flags & ~2u;
    t.feature.edge_cosines = edge_cosines;
    // 82ADE66C..: lengths are the twice-refined reciprocal of the refined
    // inverse length, not squared length multiplied by inverse length.
    for (std::size_t i = 0; i < 3; ++i) t.edge_lengths[i] = reciprocal(inverse[i]);
    t.fatness = fatness;
    return t;
}

Triangle transform_triangle_volume(std::array<Vector3, 3> vertices, float fatness,
                                   std::array<float, 3> edge_cosines, std::uint32_t volume_flags, Basis3 basis,
                                   Vector3 translation) {
    const Triangle local = triangle_from_volume(vertices, fatness, edge_cosines, volume_flags);
    std::array<V, 3> axes;
    for (std::size_t i = 0; i < 3; ++i) {
        axes[i] = V{basis.columns[i][0], basis.columns[i][1], basis.columns[i][2], 0.0f};
    }
    std::array<Vector3, 3> world_vertices;
    for (std::size_t i = 0; i < 3; ++i) {
        const Vector3 v = vertices[i];
        world_vertices[i] =
            xyz(madd(axes[2], v.z, madd(axes[1], v.y, madd(axes[0], v.x, lanes(translation, 1.0f)))));
    }
    const Vector3 n = local.feature.normal;
    const Vector3 normal = xyz(madd(axes[2], n.z, madd(axes[1], n.y, scale(axes[0], n.x))));
    Triangle result = triangle_from_volume(world_vertices, fatness, edge_cosines, volume_flags);
    result.feature.normal = normal;
    return result;
}

std::pair<std::array<std::uint32_t, 48>, PrimitiveKind> pack(const ContactPrimitive& shape) {
    std::array<std::uint32_t, 48> gp{};
    PrimitiveKind kind;
    float radius;
    std::uint32_t counts;
    std::uint32_t id;
    if (const auto* sphere = std::get_if<Sphere>(&shape.value)) {
        put(gp.data(), 0, lanes(sphere->center, 1.0f));
        kind = PrimitiveKind::Sphere;
        radius = sphere->radius;
        counts = 0;
        id = 1;
    } else if (const auto* capsule = std::get_if<ContactPrimitive::Capsule>(&shape.value)) {
        put(gp.data(), 0, lanes(capsule->center, 1.0f));
        put(gp.data(), 16, lanes(capsule->axis, 0.0f));
        gp[28] = rs::to_bits(capsule->half_length);
        kind = PrimitiveKind::Capsule;
        radius = capsule->radius;
        counts = 0x00010000u;
        id = 2;
    } else if (const auto* triangle = std::get_if<Triangle>(&shape.value)) {
        put(gp.data(), 0, lanes(triangle->vertices[0], 1.0f));
        put(gp.data(), 4, lanes(triangle->feature.normal, 0.0f));
        put(gp.data(), 8, lanes(triangle->vertices[1], 1.0f));
        put(gp.data(), 12, lanes(triangle->vertices[2], 1.0f));
        for (std::size_t i = 0; i < 3; ++i) {
            put(gp.data(), 16 + i * 4, lanes(triangle->feature.edges[i], 0.0f));
            gp[28 + i] = rs::to_bits(triangle->edge_lengths[i]);
            gp[38 + i] = rs::to_bits(triangle->feature.edge_cosines[i]);
        }
        gp[37] = triangle->feature.flags;
        kind = PrimitiveKind::Triangle;
        radius = triangle->fatness;
        counts = 0x01030000u;
        id = 3;
    } else {
        const auto& box = std::get<ContactPrimitive::RoundedBox>(shape.value);
        put(gp.data(), 0, lanes(box.center, 1.0f));
        const float half[3] = {box.half_extents.x, box.half_extents.y, box.half_extents.z};
        for (std::size_t i = 0; i < 3; ++i) {
            const auto& v = box.basis.columns[i];
            const V axis{v[0], v[1], v[2], 0.0f};
            put(gp.data(), 4 + i * 4, axis);
            put(gp.data(), 16 + i * 4, axis);
            gp[28 + i] = rs::to_bits(half[i]);
        }
        kind = PrimitiveKind::Box;
        radius = box.radius;
        counts = 0x03030000u;
        id = 4;
    }
    gp[32] = rs::to_bits(radius);
    gp[35] = counts;
    gp[36] = id;
    return {gp, kind};
}

std::array<float, 4> lanes(Vector3 v, float w) { return {v.x, v.y, v.z, w}; }
Vector3 xyz(std::array<float, 4> v) { return Vector3{v[0], v[1], v[2]}; }

}  // namespace s3::physics::world_contact::primitive_query::geometry
