// Port of skate-core/src/physics/collision/world_contact.rs.
#include "s3/core/physics/collision/world_contact.hpp"

#include "s3/core/physics/collision/arithmetic.hpp"

namespace s3::physics::collision::world_contact {

using namespace arithmetic;
using fixup::fix_up_triangle;
using fixup::TriangleFixup;
using sphere_triangle::build_prism;

float world_separation_limit(Vector3 velocity, Vector3 triangle_normal, float padding, float maximum) {
    const float approach = dot(neg(triangle_normal), scale(velocity, rs::from_bits(0x3C888889u)));
    const float positive = -approach >= 0.0f ? 0.0f : approach;
    const float limited = maximum - positive >= 0.0f ? positive : maximum;
    return limited + padding;
}

std::optional<WorldContact> sphere_triangle_world_contact(Sphere sphere, Triangle triangle,
                                                          Vector3 velocity, WorldContactSettings settings) {
    const float separation = world_separation_limit(velocity, triangle.feature.normal,
                                                    settings.volume_padding,
                                                    settings.maximum_separating_distance);
    const auto prism = build_prism(sphere, triangle, separation, false, true);
    if (!prism) return std::nullopt;
    Vector3 normal = prism->normal;
    ContactPair points[1] = {prism->raw_points};
    if (!fix_up_triangle(triangle.feature, normal, points, 1,
                         TriangleFixup{true, settings.edge_cos_bend_normal_threshold,
                                       settings.convexity_epsilon, settings.is_object})) {
        return std::nullopt;
    }
    // 8277B9E0..BA7C: fixup precedes publication. Both fatness operations
    // use separate multiply and add/subtract (the Volume query uses an FMA).
    const Vector3 a = scale(normal, sphere.radius);
    return WorldContact{
        neg(normal),
        ContactPair{
            Vector3{points[0].a.x + a.x, points[0].a.y + a.y, points[0].a.z + a.z},
            sub(points[0].b, scale(normal, triangle.fatness)),
        },
    };
}

}  // namespace s3::physics::collision::world_contact
