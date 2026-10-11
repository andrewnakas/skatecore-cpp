// Port of skate-core/src/physics/world_contact/primitive_pair.rs.
#include "s3/core/physics/world_contact/primitive_pair.hpp"

#include "s3/core/physics/world_contact/dispatch.hpp"
#include "s3/core/physics/world_contact/prism_math.hpp"
#include "s3/core/physics/world_contact/projection.hpp"

namespace s3::physics::world_contact::primitive_pair {

using namespace prism_math;
using collision::ContactPair;
using collision::fix_up_triangle;
using collision::Triangle;
using collision::TriangleFixup;
using dispatch::find_feature_intersection_prism;
using math::Vector3;
using primitive_query::maximum;
using primitive_query::geometry::pack;
using primitive_query::geometry::xyz;
using projection::best_separating_direction;
using projection::PrimitiveKind;
using projection::project_direction;

namespace {
std::array<std::uint32_t, 4> flip_bits(std::array<std::uint32_t, 4> v) {
    for (auto& w : v) w ^= 0x80000000u;
    return v;
}

/// Exact non-cylinder entries of original dispatch table 82FD56F0.
std::pair<float, std::array<std::uint32_t, 4>> separating_direction(const std::array<std::uint32_t, 48>& a,
                                                                    PrimitiveKind a_kind,
                                                                    const std::array<std::uint32_t, 48>& b,
                                                                    PrimitiveKind b_kind) {
    std::pair<std::array<std::uint32_t, 4>, std::array<std::uint32_t, 4>> r;
    if (a_kind == PrimitiveKind::Sphere && b_kind == PrimitiveKind::Sphere) {
        // Specialized 82ACE9C0. The generic no-edge fallback is Z, so it
        // cannot substitute for this center-to-center sphere-pair axis.
        const V delta = sub(load(b, 0), load(a, 0));
        const float squared = dot(delta, delta);
        const float inverse = inverse_length(squared);
        const float length = squared == 0.0f ? 0.0f : squared * inverse;
        const std::uint32_t lb = rs::to_bits(length);
        r = {{lb, lb, lb, lb}, bits(scale(delta, inverse))};
    } else if (a_kind == PrimitiveKind::Box && b_kind == PrimitiveKind::Box) {
        r = primitive_query::box_box_sat::box_box(a, b);
    } else if (a_kind == PrimitiveKind::Triangle && b_kind == PrimitiveKind::Box) {
        r = primitive_query::triangle_box_sat::triangle_box(a, b);
    } else if (a_kind == PrimitiveKind::Box && b_kind == PrimitiveKind::Triangle) {
        const auto t = primitive_query::triangle_box_sat::triangle_box(b, a);
        r = {t.first, flip_bits(t.second)};
    } else {
        r = best_separating_direction(a, a_kind, b, b_kind);
    }
    return {rs::from_bits(r.first[0]), r.second};
}
}  // namespace

std::optional<PrimitiveContactManifold> primitive_pair_contacts(const ContactPrimitive& primitive_a,
                                                                const ContactPrimitive& primitive_b,
                                                                PrimitivePairSettings settings) {
    const auto [a, a_kind] = pack(primitive_a);
    const auto [b, b_kind] = pack(primitive_b);
    const auto [separation, initial_normal] = separating_direction(a, a_kind, b, b_kind);
    // 8277A720..744 computes the pair padding before 82AD43F8..4414 adds
    // geometric fatness, preserving all four scalar additions in order.
    const float limit = (settings.padding_b + settings.padding_a) + settings.additional_padding;
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
        // 82AD44B0..4540 compares *length*, not squared length. The source
        // computes both reciprocal-square-root refinements before selecting
        // zero length for a zero squared distance.
        const V delta = sub(load(prism, 64), load(prism, 0));
        const float squared = dot(delta, delta);
        const float inverse = inverse_length(squared);
        const float length = squared == 0.0f ? 0.0f : squared * inverse;
        if (length > rs::from_bits(0x34000000u)) {
            store_bits(prism.data(), 128, scale(delta, inverse));
            prism[133] = 1;
        }
    }
    if (prism[133] != 0) {
        const std::array<std::uint32_t, 4> direction{prism[128], prism[129], prism[130], prism[131]};
        std::array<std::uint32_t, 12> interval_a{};
        std::array<std::uint32_t, 12> interval_b{};
        project_direction(a, a_kind, direction, interval_a);
        project_direction(b, b_kind, direction, interval_b);
        const float forward = rs::from_bits(interval_a[0]) - rs::from_bits(interval_b[4]);
        const float reverse = rs::from_bits(interval_b[0]) - rs::from_bits(interval_a[4]);
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
    // General GP query corrects either triangle, A first. The skater-specific
    // world query corrects only B and supplies a different is_object argument.
    Vector3 pair_normal = xyz(normal);
    const std::pair<const ContactPrimitive*, bool> passes[2] = {{&primitive_a, false}, {&primitive_b, true}};
    for (const auto& [primitive, reverse] : passes) {
        if (const Triangle* triangle = primitive->as_triangle()) {
            if (!fix_up_triangle(triangle->feature, pair_normal, points.data(), count,
                                 TriangleFixup{reverse, settings.edge_cos_bend_normal_threshold,
                                               settings.convexity_epsilon, false})) {
                return std::nullopt;
            }
        }
    }
    const V direction{pair_normal.x, pair_normal.y, pair_normal.z, normal[3]};
    const V offset_a = scale(direction, fat_a);
    const V offset_b = scale(direction, fat_b);
    for (std::size_t k = 0; k < count; ++k) {
        ContactPair& point = points[k];
        // 82AD46EC..4724 multiplies before adding/subtracting, not fused.
        point.a = Vector3{point.a.x + offset_a[0], point.a.y + offset_a[1], point.a.z + offset_a[2]};
        point.b = Vector3{point.b.x - offset_b[0], point.b.y - offset_b[1], point.b.z - offset_b[2]};
    }
    return PrimitiveContactManifold{Vector3{-pair_normal.x, -pair_normal.y, -pair_normal.z}, points, count};
}

}  // namespace s3::physics::world_contact::primitive_pair
