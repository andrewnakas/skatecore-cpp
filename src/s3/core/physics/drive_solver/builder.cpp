// Port of skate-core/src/physics/drive_solver/builder.rs.
#include "s3/core/physics/constraint_frames.hpp"
#include "s3/core/physics/drive_solver.hpp"
#include "s3/core/physics/native_arithmetic.hpp"

namespace s3::physics::drive_solver::builder {

namespace {

struct WorldFrame {
    RetailQuaternion orientation;
    Vector3 arm;
    Vector3 position;
};

WorldFrame world_frame(const RetailDriveBodyState& body, const RetailDriveFrame& frame) {
    const Vector3 arm = constraint_frames::transform_direction(body.basis, frame.translation);
    return WorldFrame{
        constraint_frames::compose(body.orientation, frame.orientation),
        arm,
        add(body.center_of_mass, arm),
    };
}

struct DriveCoefficients {
    float softness;
    std::array<float, 3> target_displacement;
    float maximum_strength;
};

std::array<float, 3> components_of(Vector3 v) { return {v.x, v.y, v.z}; }

/// 82AE1E68..20B8 and the matching angular block219C..237C.
/// Inputs are already scaled in their source order; the hard cap affects only
/// position error, before the rate and acceleration displacements are added.
DriveCoefficients build_coefficients(RetailDriveParams params, std::array<float, 3> position_error,
                                     std::array<float, 3> velocity_displacement,
                                     std::array<float, 3> acceleration_displacement, float time_step) {
    const float damping = time_step * params.damping;
    float position_weight;
    float velocity_weight;
    float acceleration_weight;
    if (params.drive_type == RetailDriveType::SoftDrive) {
        const float spring = (time_step * params.spring_or_max_velocity) * time_step;
        const float inverse = constraint_frames::reciprocal((1.0f + spring) + damping);
        position_weight = inverse * spring;
        velocity_weight = inverse * damping;
        acceleration_weight = inverse * (spring + damping);
    } else {
        position_weight = constraint_frames::reciprocal(1.0f + damping) * 1.0f;
        velocity_weight = 1.0f;
        acceleration_weight = 1.0f;
    }
    if (params.drive_type == RetailDriveType::HardDrive) {
        const float squared = native_arithmetic::dot3(
            {position_error[0], position_error[1], position_error[2], 0.0f},
            {position_error[0], position_error[1], position_error[2], 0.0f});
        const float maximum = time_step * params.spring_or_max_velocity;
        if (squared > maximum * maximum) {
            const float factor = maximum * constraint_frames::reciprocal_sqrt(squared);
            position_error = rs::map(position_error, [&](float v) { return v * factor; });
        }
    }
    DriveCoefficients out{};
    out.softness = acceleration_weight;
    for (std::size_t i = 0; i < 3; ++i) {
        const float rate = velocity_displacement[i] * velocity_weight;
        const float position_and_rate = rs::mul_add(position_error[i], position_weight, rate);
        out.target_displacement[i] = rs::mul_add(acceleration_displacement[i], acceleration_weight, position_and_rate);
    }
    // Type zero follows the same native strength load; its authored params
    // normally provide zero strength rather than the builder inventing it.
    out.maximum_strength = (time_step * params.max_strength) * time_step;
    return out;
}

float linear_effective_mass(const RetailDriveBodyState& frame_a, const RetailDriveBodyState& frame_b,
                            Vector3 arm_a, Vector3 arm_b, Vector3 axis) {
    float denominator = 0.0f;
    if (is_active(frame_a)) {
        const Vector3 angular = cross(arm_a, axis);
        denominator += frame_a.inverse_mass +
                       dot(angular, constraint_frames::multiply_inertia(frame_a.world_inverse_inertia, angular));
    }
    if (is_active(frame_b)) {
        const Vector3 angular = cross(arm_b, axis);
        denominator += frame_b.inverse_mass +
                       dot(angular, constraint_frames::multiply_inertia(frame_b.world_inverse_inertia, angular));
    }
    return denominator;
}

float angular_effective_mass(const RetailDriveBodyState& frame_a, const RetailDriveBodyState& frame_b,
                             Vector3 axis) {
    float denominator = 0.0f;
    if (is_active(frame_a)) {
        denominator += dot(axis, constraint_frames::multiply_inertia(frame_a.world_inverse_inertia, axis));
    }
    if (is_active(frame_b)) {
        denominator += dot(axis, constraint_frames::multiply_inertia(frame_b.world_inverse_inertia, axis));
    }
    return denominator;
}

}  // namespace

RetailDriveRows build_drive_rows(RetailDriveBodyState frame_a_body, RetailDriveBodyState frame_b_body,
                                 RetailDriveFrames frames, RetailDriveDynamics dynamics, float time_step) {
    S3_ASSERT(rs::is_finite(time_step) && time_step > 0.0f);

    const WorldFrame frame_a_world = world_frame(frame_a_body, frames.body_a);
    const WorldFrame frame_b_world = world_frame(frame_b_body, frames.body_b);
    const Vector3 arm_a = frame_a_world.arm;
    const Vector3 arm_b = frame_b_world.arm;
    const std::array<Vector3, 3> linear_axes = basis_columns(constraint_frames::basis(frame_b_world.orientation));

    const auto raw_angular = constraint_frames::quaternion_rows(frame_a_world.orientation, frame_b_world.orientation);
    const std::array<float, 3> components{raw_angular.relative.x, raw_angular.relative.y, raw_angular.relative.z};
    const auto squared = rs::map(components, [](float c) {
        return native_arithmetic::vector_min(native_arithmetic::vector_max(c * c, 0.0f), 1.0f);
    });
    // 830BDB40 selects bytes [0,4,8,8] in every word: any saturated
    // quaternion component replaces ALL three axes with frame B's basis.
    const bool singular = squared[0] == 1.0f || squared[1] == 1.0f || squared[2] == 1.0f;
    const std::array<float, 3> inverse_lengths =
        singular ? std::array<float, 3>{1.0f, 1.0f, 1.0f}
                 : rs::map(squared, [](float c) { return constraint_frames::reciprocal_sqrt(1.0f - c); });
    std::array<Vector3, 3> angular_axes{};
    std::array<float, 3> angular_position_error{};
    for (std::size_t i = 0; i < 3; ++i) {
        angular_axes[i] = scale(singular ? linear_axes[i] : raw_angular.axes[i], inverse_lengths[i]);
    }
    for (std::size_t i = 0; i < 3; ++i) {
        angular_position_error[i] = (components[i] * inverse_lengths[i]) * 2.0f;
    }

    const Vector3 linear_position_error = sub(frame_b_world.position, frame_a_world.position);
    const Vector3 linear_velocity_displacement = sub(
        scale(point_rate(frame_b_body.linear_velocity, frame_b_body.angular_velocity, arm_b), time_step),
        scale(point_rate(frame_a_body.linear_velocity, frame_a_body.angular_velocity, arm_a), time_step));
    const Vector3 linear_acceleration_displacement = sub(
        scale(scale(point_rate(frame_b_body.force_acceleration, frame_b_body.torque_acceleration, arm_b), time_step),
              time_step),
        scale(scale(point_rate(frame_a_body.force_acceleration, frame_a_body.torque_acceleration, arm_a), time_step),
              time_step));
    const Vector3 angular_velocity_displacement =
        scale(sub(frame_b_body.angular_velocity, frame_a_body.angular_velocity), time_step);
    const Vector3 angular_acceleration_displacement =
        scale(scale(sub(frame_b_body.torque_acceleration, frame_a_body.torque_acceleration), time_step), time_step);
    std::array<float, 3> linear_inverse_effective_mass{};
    for (std::size_t index = 0; index < 3; ++index) {
        linear_inverse_effective_mass[index] = constraint_frames::reciprocal(
            linear_effective_mass(frame_a_body, frame_b_body, arm_a, arm_b, linear_axes[index]));
    }
    std::array<float, 3> angular_inverse_effective_mass{};
    for (std::size_t index = 0; index < 3; ++index) {
        angular_inverse_effective_mass[index] =
            constraint_frames::reciprocal(angular_effective_mass(frame_a_body, frame_b_body, angular_axes[index]));
    }

    const DriveCoefficients linear_coefficients = build_coefficients(
        dynamics.linear, components_of(linear_position_error), components_of(linear_velocity_displacement),
        components_of(linear_acceleration_displacement), time_step);
    // Linear weights combine in world space before the projection (20A8..20D0).
    const auto& displacement = linear_coefficients.target_displacement;
    const std::array<float, 3> linear_target =
        project(Vector3{displacement[0], displacement[1], displacement[2]}, linear_axes);
    const DriveCoefficients angular_coefficients = build_coefficients(
        dynamics.angular, angular_position_error, project(angular_velocity_displacement, angular_axes),
        project(angular_acceleration_displacement, angular_axes), time_step);
    RetailDriveRows rows;
    rows.frame_a_body = frame_a_body;
    rows.frame_b_body = frame_b_body;
    rows.arm_a = arm_a;
    rows.arm_b = arm_b;
    rows.linear_axes = linear_axes;
    rows.angular_axes = angular_axes;
    rows.linear_inverse_effective_mass = linear_inverse_effective_mass;
    rows.angular_inverse_effective_mass = angular_inverse_effective_mass;
    rows.linear_softness = linear_coefficients.softness;
    rows.angular_softness = angular_coefficients.softness;
    for (std::size_t i = 0; i < 3; ++i) {
        rows.linear_target_impulse[i] = linear_target[i] * linear_inverse_effective_mass[i];
    }
    for (std::size_t i = 0; i < 3; ++i) {
        rows.angular_target_impulse[i] =
            angular_coefficients.target_displacement[i] * angular_inverse_effective_mass[i];
    }
    rows.linear_maximum_impulse =
        rs::map(linear_inverse_effective_mass, [&](float v) { return v * linear_coefficients.maximum_strength; });
    rows.angular_maximum_impulse =
        rs::map(angular_inverse_effective_mass, [&](float v) { return v * angular_coefficients.maximum_strength; });
    rows.accumulated_linear_impulse = {0.0f, 0.0f, 0.0f};
    rows.accumulated_angular_impulse = {0.0f, 0.0f, 0.0f};
    return rows;
}

}  // namespace s3::physics::drive_solver::builder
