// Port of skate-core/src/physics/world_contact/primitive_query.rs.
#include "s3/core/physics/world_contact/primitive_query.hpp"

#include "s3/core/physics/world_contact/box_feature.hpp"
#include "s3/core/physics/world_contact/dispatch.hpp"
#include "s3/core/physics/world_contact/feature.hpp"
#include "s3/core/physics/world_contact/prism_math.hpp"
#include "s3/core/physics/world_contact/projection.hpp"
#include "s3/core/physics/world_contact/triangle_feature.hpp"

namespace s3::physics::world_contact::primitive_query {

using namespace prism_math;
using collision::fix_up_triangle;
using collision::TriangleFixup;
using collision::world_separation_limit;
using dispatch::find_feature_intersection_prism;
using geometry::lanes;
using geometry::pack;
using geometry::xyz;
using projection::best_separating_direction;
using projection::project_direction;

namespace {
std::array<std::uint32_t, 4> flip_bits(std::array<std::uint32_t, 4> v) {
    for (auto& w : v) w ^= 0x80000000u;
    return v;
}

std::optional<PrimitiveContactManifold> contact_points(const std::array<std::uint32_t, 48>& a,
                                                       PrimitiveKind a_kind,
                                                       const std::array<std::uint32_t, 48>& b,
                                                       PrimitiveKind b_kind, Triangle triangle, float separation,
                                                       std::array<std::uint32_t, 4> initial_normal, float limit,
                                                       WorldContactSettings settings) {
    const float fat_a = rs::from_bits(a[32]);
    const float fat_b = rs::from_bits(b[32]);
    if (separation > (fat_b + limit) + fat_a) {
        return std::nullopt;
    }
    MaximumFeature feature_a = maximum(a, a_kind, 1, initial_normal);
    MaximumFeature feature_b = maximum(b, b_kind, 0, flip_bits(initial_normal));
    FeaturePrism prism{};
    if (find_feature_intersection_prism(prism, feature_a, feature_b, initial_normal) == 0) {
        return std::nullopt;
    }
    const std::size_t count = prism[132];
    V normal = rs::from_bits(initial_normal);
    if (count == 1) {
        const V delta = sub(load(prism, 64), load(prism, 0));
        const float squared = dot(delta, delta);
        const float inverse = inverse_length(squared);
        const float length = squared == 0.0f ? 0.0f : squared * inverse;
        // This world-query gate compares length. The general Volume query
        // has a different squared-distance gate and publication sequence.
        if (length > rs::from_bits(0x34000000u)) {
            store_bits(prism.data(), 128, scale(delta, inverse));
            prism[133] = 1;
        }
    }
    if (prism[133] != 0) {
        const std::array<std::uint32_t, 4> direction{prism[128], prism[129], prism[130], prism[131]};
        std::array<std::uint32_t, 12> ai{};
        std::array<std::uint32_t, 12> bi{};
        project_direction(a, a_kind, direction, ai);
        project_direction(b, b_kind, direction, bi);
        const float forward = rs::from_bits(ai[0]) - rs::from_bits(bi[4]);
        const float reverse = rs::from_bits(bi[0]) - rs::from_bits(ai[4]);
        const bool flip = forward > reverse;
        const float refined = flip ? forward : reverse;
        if (refined >= separation) {
            normal = rs::from_bits(flip ? flip_bits(direction) : direction);
        }
    }
    std::array<ContactPair, 16> points;
    for (auto& p : points) p = ContactPair{Vector3::ZERO, Vector3::ZERO};
    for (std::size_t i = 0; i < count; ++i) {
        points[i].a = xyz(load(prism, 4 * i));
        points[i].b = xyz(load(prism, 64 + 4 * i));
    }
    Vector3 pair_normal = xyz(normal);
    if (!fix_up_triangle(triangle.feature, pair_normal, points.data(), count,
                         TriangleFixup{true, settings.edge_cos_bend_normal_threshold, settings.convexity_epsilon,
                                       settings.is_object})) {
        return std::nullopt;
    }
    const V n = lanes(pair_normal, 0.0f);
    const V a_offset = scale(n, fat_a);
    const V b_offset = scale(n, fat_b);
    for (std::size_t k = 0; k < count; ++k) {
        ContactPair& point = points[k];
        // Separate multiply and add/subtract in8277BA50..BA60, after fixup.
        const V pa = lanes(point.a, 1.0f);
        point.a = xyz(V{pa[0] + a_offset[0], pa[1] + a_offset[1], pa[2] + a_offset[2], pa[3] + a_offset[3]});
        point.b = xyz(sub(lanes(point.b, 1.0f), b_offset));
    }
    return PrimitiveContactManifold{xyz(V{-n[0], -n[1], -n[2], -n[3]}), points, count};
}
}  // namespace

std::optional<PrimitiveContactManifold> primitive_triangle_world_contacts(const ContactPrimitive& primitive,
                                                                          Triangle triangle, Vector3 velocity,
                                                                          WorldContactSettings settings) {
    const float limit = world_separation_limit(velocity, triangle.feature.normal, settings.volume_padding,
                                               settings.maximum_separating_distance);
    const auto [a, a_kind] = pack(primitive);
    const auto [b, b_kind] = pack(ContactPrimitive(triangle));
    std::array<std::uint32_t, 4> separation, normal;
    if (a_kind == PrimitiveKind::Box) {
        // Reversed specialized entry82ACE968 calls triangle/box then flips
        // every sign bit. Native pair direction always points A toward B.
        const auto r = triangle_box_sat::triangle_box(b, a);
        separation = r.first;
        normal = flip_bits(r.second);
    } else {
        const auto r = best_separating_direction(a, a_kind, b, b_kind);
        separation = r.first;
        normal = r.second;
    }
    return contact_points(a, a_kind, b, b_kind, triangle, rs::from_bits(separation[0]), normal, limit, settings);
}

MaximumFeature maximum(const std::array<std::uint32_t, 48>& gp, PrimitiveKind kind, std::uint32_t mode,
                       std::array<std::uint32_t, 4> direction) {
    MaximumFeature feature{};
    switch (kind) {
        case PrimitiveKind::Sphere:
            // Complete sphere callback82ADD7E0: center, point type, header0.
            for (std::size_t i = 0; i < 4; ++i) feature[136 + i] = gp[i];
            feature[140] = 0;
            feature[0] = 0;
            break;
        case PrimitiveKind::Capsule: {
            std::array<std::uint32_t, 16> scratch{};
            feature::capsule_maximum_feature(gp, direction, feature, scratch);
            break;
        }
        case PrimitiveKind::Triangle:
            triangle_feature::triangle_maximum_feature(gp, mode, direction, feature);
            break;
        case PrimitiveKind::Box:
            box_feature::box_maximum_feature(gp, mode, direction, feature, {0, 0, 0, 0});
            break;
    }
    return feature;
}

}  // namespace s3::physics::world_contact::primitive_query
