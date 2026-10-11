// Port of skate-core/src/physics/contact.rs.
#include "s3/core/physics/contact.hpp"

#include "s3/core/physics/reciprocal_sqrt.hpp"

namespace s3::physics::contact {

namespace {

float material_scalar_store(float value) {
    if (rs::is_nan(value)) {
        return rs::from_bits(rs::to_bits(value) | 0x00400000u);
    }
    return value;
}

Vector3 sub(Vector3 left, Vector3 right) {
    return Vector3{left.x - right.x, left.y - right.y, left.z - right.z};
}

Vector3 scale(Vector3 vector, float scalar) {
    return Vector3{
        rs::mul_add(vector.x, scalar, 0.0f),
        rs::mul_add(vector.y, scalar, 0.0f),
        rs::mul_add(vector.z, scalar, 0.0f),
    };
}

float length_squared(Vector3 vector) { return dot(vector, vector); }

/// Exact normal-only fallback selected by the TU3 VMX block.
std::pair<Vector3, float> fallback_tangent(Vector3 normal) {
    const Vector3 candidate_x{
        rs::mul_add(-normal.x, normal.x, 1.0f),
        rs::mul_add(-normal.y, normal.x, 0.0f),
        rs::mul_add(-normal.z, normal.x, 0.0f),
    };
    const Vector3 candidate_z{
        rs::mul_add(-normal.x, normal.z, 0.0f),
        rs::mul_add(-normal.y, normal.z, 0.0f),
        rs::mul_add(-normal.z, normal.z, 1.0f),
    };
    if (rs::mul_add(-normal.x, normal.x, CONTACT_FALLBACK_HALF) >= -0.0f) {
        return {candidate_x, candidate_x.x};
    }
    return {Vector3{-candidate_z.x, -candidate_z.y, -candidate_z.z}, candidate_z.z};
}

Vector3 point_velocity(Vector3 linear, Vector3 omega, Vector3 arm) {
    // Native folds the linear velocity into two fused cross-product terms.
    return Vector3{
        rs::mul_add(arm.z, omega.y, rs::mul_add(-arm.y, omega.z, linear.x)),
        rs::mul_add(arm.x, omega.z, rs::mul_add(-arm.z, omega.x, linear.y)),
        rs::mul_add(arm.y, omega.x, rs::mul_add(-arm.x, omega.y, linear.z)),
    };
}

}  // namespace

RetailContactMaterial combine_contact_materials(RetailContactMaterial material_a,
                                                RetailContactMaterial material_b) {
    const float static_friction = material_a.static_friction > material_b.static_friction
                                      ? material_a.static_friction
                                      : material_b.static_friction;
    const float dynamic_friction = material_a.dynamic_friction > material_b.dynamic_friction
                                       ? material_a.dynamic_friction
                                       : material_b.dynamic_friction;
    const float restitution = material_a.restitution < material_b.restitution
                                  ? material_a.restitution
                                  : material_b.restitution;
    return RetailContactMaterial{
        material_scalar_store(static_friction),
        material_scalar_store(dynamic_friction),
        material_scalar_store(restitution),
    };
}

RetailContact generate_contact(RetailContactInput input, RetailContactBodyState body_a,
                               RetailContactBodyState body_b) {
    const Vector3 arm_a = sub(input.position_on_a, body_a.center_of_mass);
    const Vector3 arm_b = sub(input.position_on_b, body_b.center_of_mass);
    const Vector3 point_velocity_a = point_velocity(body_a.linear_velocity, body_a.angular_velocity, arm_a);
    const Vector3 point_velocity_b = point_velocity(body_b.linear_velocity, body_b.angular_velocity, arm_b);
    const Vector3 relative_velocity = sub(point_velocity_b, point_velocity_a);

    const Vector3 velocity_tangent = cross(relative_velocity, input.normal);
    const float velocity_tangent_squared = length_squared(velocity_tangent);
    const auto [unnormalized_tangent, tangent_squared] =
        velocity_tangent_squared >= CONTACT_TANGENT_MINIMUM_SQUARED
            ? std::pair<Vector3, float>{velocity_tangent, velocity_tangent_squared}
            : fallback_tangent(input.normal);

    // TU3 issues one reciprocal-square-root estimate and applies it to both
    // axes. `tangent_1` therefore assumes the collision normal is unit length.
    const float inverse_length = reciprocal_sqrt::estimate(tangent_squared);
    const Vector3 tangent_0 = scale(unnormalized_tangent, inverse_length);
    // The second cross product rounds the positive product first, then uses
    // vnmsubfp for the negative term; tangent_0 uses the opposite order.
    const Vector3 n = input.normal;
    const Vector3 t = unnormalized_tangent;
    const Vector3 tangent_1 = scale(
        Vector3{
            rs::mul_add(-t.y, n.z, rs::mul_add(t.z, n.y, 0.0f)),
            rs::mul_add(-t.z, n.x, rs::mul_add(t.x, n.z, 0.0f)),
            rs::mul_add(-t.x, n.y, rs::mul_add(t.y, n.x, 0.0f)),
        },
        inverse_length);

    return RetailContact{
        input.position_on_a,
        body_a.contact_body_id,
        input.position_on_b,
        body_b.contact_body_id,
        input.normal,
        input.restitution,
        tangent_0,
        input.static_friction,
        tangent_1,
        input.dynamic_friction,
        relative_velocity,
        input.tag,
        RetailContactWorkspace::from(body_a),
        RetailContactWorkspace::from(body_b),
    };
}

Vector3 cross(Vector3 left, Vector3 right) {
    return Vector3{
        rs::mul_add(left.y, right.z, rs::mul_add(-left.z, right.y, 0.0f)),
        rs::mul_add(left.z, right.x, rs::mul_add(-left.x, right.z, 0.0f)),
        rs::mul_add(left.x, right.y, rs::mul_add(-left.y, right.x, 0.0f)),
    };
}

float dot(Vector3 left, Vector3 right) {
    return rs::mul_add(left.z, right.z,
                       rs::mul_add(left.y, right.y, rs::mul_add(left.x, right.x, 0.0f)));
}

}  // namespace s3::physics::contact
