// Port of skate-core/src/physics/solver/contact_build/geometry.rs.
#include "s3/core/physics/solver/contact_build.hpp"

namespace s3::physics::solver::contact_build::geometry {

namespace {

std::array<float, 3> components(Vector3 v) { return {v.x, v.y, v.z}; }

Vector3 difference(Vector3 a, Vector3 b) { return Vector3{a.x - b.x, a.y - b.y, a.z - b.z}; }

std::array<float, 3> cross(Vector3 arm, Vector3 axis) {
    return {
        rs::mul_add(-arm.z, axis.y, rs::mul_add(arm.y, axis.z, 0.0f)),
        rs::mul_add(-arm.x, axis.z, rs::mul_add(arm.z, axis.x, 0.0f)),
        rs::mul_add(-arm.y, axis.x, rs::mul_add(arm.x, axis.y, 0.0f)),
    };
}

std::array<float, 4> inertia_response(Vector3 full, Vector3 split, float mass, std::array<float, 3> jacobian) {
    const std::array<std::array<float, 4>, 3> columns{{
        {full.x, full.y, full.z, mass},
        {full.y, split.y, split.z, mass},
        {full.z, split.z, split.x, mass},
    }};
    std::array<float, 4> out{};
    for (std::size_t component = 0; component < 4; ++component) {
        const float first = rs::mul_add(columns[0][component], jacobian[0], 0.0f);
        const float second = rs::mul_add(columns[1][component], jacobian[1], first);
        out[component] = rs::mul_add(columns[2][component], jacobian[2], second);
    }
    return out;
}

std::array<float, 3> project(const std::array<Vector3, 3>& axes, Vector3 value) {
    std::array<float, 3> out{};
    for (std::size_t i = 0; i < 3; ++i) {
        const Vector3& axis = axes[i];
        const float x = rs::mul_add(axis.x, value.x, 0.0f);
        const float xy = rs::mul_add(axis.y, value.y, x);
        out[i] = rs::mul_add(axis.z, value.z, xy);
    }
    return out;
}

}  // namespace

ContactPreparation prepare(const input::ContactInput& input, float time_step) {
    std::array<Vector3, 2> arms{};
    for (std::size_t body = 0; body < 2; ++body) {
        arms[body] = difference(input.positions[body], input.bodies[body].center);
    }
    std::array<bool, 2> active{};
    for (std::size_t body = 0; body < 2; ++body) active[body] = (input.bodies[body].state & 4u) == 4u;
    std::array<float, 2> inverse_mass{};
    for (std::size_t body = 0; body < 2; ++body) {
        inverse_mass[body] = active[body] ? input.bodies[body].inverse_mass : 0.0f;
    }
    std::array<std::array<std::array<float, 3>, 3>, 2> jacobians{};
    for (std::size_t body = 0; body < 2; ++body) {
        for (std::size_t axis = 0; axis < 3; ++axis) jacobians[body][axis] = cross(arms[body], input.axes[axis]);
    }
    std::array<std::array<std::array<float, 4>, 3>, 2> responses{};
    for (std::size_t body = 0; body < 2; ++body) {
        const Vector3 full = active[body] ? input.bodies[body].inverse_inertia_full : Vector3::ZERO;
        const Vector3 split = active[body] ? input.bodies[body].inverse_inertia_split : Vector3::ZERO;
        for (std::size_t axis = 0; axis < 3; ++axis) {
            responses[body][axis] = inertia_response(full, split, inverse_mass[body], jacobians[body][axis]);
        }
    }
    std::array<float, 3> effective_mass{};
    for (std::size_t axis = 0; axis < 3; ++axis) {
        std::array<float, 3> products{};
        for (std::size_t component = 0; component < 3; ++component) {
            const float a = rs::mul_add(jacobians[0][axis][component], responses[0][axis][component], 0.0f);
            products[component] = rs::mul_add(jacobians[1][axis][component], responses[1][axis][component], a);
        }
        effective_mass[axis] = (products[2] + (products[0] + inverse_mass[0])) + (products[1] + inverse_mass[1]);
    }
    std::array<Vector3, 2> point_acceleration{};
    for (std::size_t body = 0; body < 2; ++body) {
        if (!active[body]) {
            point_acceleration[body] = Vector3::ZERO;
            continue;
        }
        const Vector3 arm = arms[body];
        const Vector3 force = input.bodies[body].force_acceleration;
        const Vector3 torque = input.bodies[body].torque_acceleration;
        point_acceleration[body] = Vector3{
            rs::mul_add(arm.z, torque.y, rs::mul_add(-arm.y, torque.z, force.x)),
            rs::mul_add(arm.x, torque.z, rs::mul_add(-arm.z, torque.x, force.y)),
            rs::mul_add(arm.y, torque.x, rs::mul_add(-arm.x, torque.y, force.z)),
        };
    }
    const Vector3 separation = difference(input.positions[1], input.positions[0]);
    const Vector3 relative_acceleration = difference(point_acceleration[1], point_acceleration[0]);
    const auto scaled_velocity =
        rs::map(components(input.relative_velocity), [&](float v) { return rs::mul_add(time_step, v, 0.0f); });
    const auto acceleration = rs::map(components(relative_acceleration),
                                      [&](float v) { return rs::mul_add(v, time_step * time_step, 0.0f); });
    std::array<float, 3> predicted{};
    for (std::size_t i = 0; i < 3; ++i) {
        predicted[i] = scaled_velocity[i] + (components(separation)[i] + acceleration[i]);
    }
    const auto restitution =
        rs::map(scaled_velocity, [&](float v) { return rs::mul_add(-v, input.restitution, 0.0f); });
    ContactPreparation p;
    p.arms = arms;
    p.axes = input.axes;
    p.active = active;
    p.inverse_mass = inverse_mass;
    p.point_acceleration = point_acceleration;
    p.angular_response_a = responses[0];
    p.angular_response_b = responses[1];
    p.effective_mass = effective_mass;
    p.separation_projection = project(input.axes, separation);
    p.restitution_projection = project(input.axes, Vector3{restitution[0], restitution[1], restitution[2]});
    p.predicted_separation_projection = project(input.axes, Vector3{predicted[0], predicted[1], predicted[2]});
    p.reaction_ids = {input.bodies[0].reaction_id, input.bodies[1].reaction_id};
    p.body_ids = input.body_ids;
    p.static_friction_bits = input.static_friction_bits;
    p.dynamic_friction_bits = input.dynamic_friction_bits;
    p.contact_tag = input.contact_tag;
    p.combined_state_bit_8 = (input.bodies[0].state | input.bodies[1].state) & 8u;
    return p;
}

}  // namespace s3::physics::solver::contact_build::geometry
