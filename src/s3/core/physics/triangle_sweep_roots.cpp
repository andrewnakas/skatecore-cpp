// Port of skate-core/src/physics/triangle_sweep_roots.rs.
#include "s3/core/physics/triangle_sweep_roots.hpp"

#include "s3/core/physics/triangle_query.hpp"

namespace s3::physics::triangle_sweep_roots {

using triangle_query::cross;
using triangle_query::dot;
using triangle_query::sub;

Root sphere(Vector3 start, Vector3 delta, Vector3 center, float radius) {
    const Vector3 toward_center = sub(center, start);
    const float radius_squared = radius * radius;
    if (dot(toward_center, toward_center) < radius_squared) {
        return Root::Hit(Fraction{0.0f, 1.0f});
    }
    const float approach = dot(toward_center, delta);
    if (!(approach > 0.0f)) {
        return Root::Away();
    }
    const float delta_squared = dot(delta, delta);
    const Vector3 perpendicular = cross(toward_center, delta);
    const float discriminant = rs::mul_add(delta_squared, radius_squared, -dot(perpendicular, perpendicular));
    if (discriminant < 0.0f) {
        return Root::Miss();
    }
    const float overrun = approach - delta_squared;
    if (overrun > 0.0f && overrun * overrun > discriminant) {
        return Root::Miss();
    }
    return Root::Hit(Fraction{approach - rs::sqrt(discriminant), delta_squared});
}

Root cylinder(Vector3 start, Vector3 delta, Vector3 origin, Vector3 edge, float radius) {
    const Vector3 perpendicular = cross(sub(origin, start), edge);
    const float distance_squared = dot(perpendicular, perpendicular);
    const float radius_squared = (dot(edge, edge) * radius) * radius;
    if (distance_squared < radius_squared) {
        return Root::Hit(Fraction{0.0f, 1.0f});
    }
    const Vector3 delta_perpendicular = cross(delta, edge);
    const float approach = dot(perpendicular, delta_perpendicular);
    if (!(approach > 0.0f)) {
        return Root::Away();
    }
    const float delta_squared = dot(delta_perpendicular, delta_perpendicular);
    const float residual = rs::mul_add(approach, approach, -(delta_squared * distance_squared));
    const float discriminant = rs::mul_add(delta_squared, radius_squared, residual);
    if (discriminant < 0.0f) {
        return Root::Miss();
    }
    const float overrun = approach - delta_squared;
    // Cylinder rejects equality here; the sphere helper uses strict greater.
    if (!(overrun < 0.0f) && !(overrun * overrun < discriminant)) {
        return Root::Miss();
    }
    return Root::Hit(Fraction{approach - rs::sqrt(discriminant), delta_squared});
}

}  // namespace s3::physics::triangle_sweep_roots
