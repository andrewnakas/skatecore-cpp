// Port of skate-core/src/physics/joint_builder.rs.
#include "s3/core/physics/joint_builder.hpp"

#include "s3/core/physics/constraint_frames.hpp"
#include "s3/core/physics/native_arithmetic.hpp"

namespace s3::physics::joint_builder {

namespace {
void write_vector(RetailJointJacobian& output, std::size_t vector, std::array<float, 4> value) {
    const std::size_t start = vector * 4;
    for (std::size_t lane = 0; lane < 4; ++lane) {
        output.words[start + lane] = rs::to_bits(value[lane]);
    }
}
}  // namespace

RetailJointJacobian build_retail_joint_jacobian(RetailJointBuildInput input) {
    S3_ASSERT(rs::is_finite(input.time_step) && input.time_step > 0.0f);

    const JointParameters parameters = decode_parameters(input.parameters);
    const JointFrames frames = decode_frames(input.frames);
    const bool active_a = (input.body_a.state & tu3::ACTIVE_BODY) != 0;
    const bool active_b = (input.body_b.state & tu3::ACTIVE_BODY) != 0;

    // Observed composition order: body orientation followed by local frame.
    const RetailQuaternion world_orientation_a = quaternion_multiply(input.body_a.orientation, frames.orientation_a);
    const RetailQuaternion world_orientation_b = quaternion_multiply(input.body_b.orientation, frames.orientation_b);
    const RetailQuaternion world_linear_orientation =
        quaternion_multiply(input.body_b.orientation, frames.linear_orientation_b);
    const Basis3 angular_basis_a = basis_from_quaternion_retail(world_orientation_a);
    const Basis3 angular_basis_b = basis_from_quaternion_retail(world_orientation_b);
    const Basis3 linear_basis = basis_from_quaternion_retail(world_linear_orientation);

    const Vector3 arm_a = multiply_basis(input.body_a.basis, frames.anchor_a);
    const Vector3 arm_b = multiply_basis(input.body_b.basis, frames.anchor_b);
    const std::array<Vector3, 3> linear_axes = basis_columns(linear_basis);
    // Retained matrix product: the original gathers an RQD quaternion before
    // constructing this matrix. The VA extension of 82AE3D98 (10242A35) is
    // not independently decoded, so its rounding path remains unverified.
    const auto relative_matrix = relative_basis(angular_basis_a, angular_basis_b);
    const Rqd rqd = create_rqd(world_orientation_a, world_orientation_b);
    const auto [angular_axes, angular_raw_low, angular_raw_high] =
        angular_rows(parameters, angular_basis_a, angular_basis_b, relative_matrix, rqd);

    const float inverse_mass_a = active_a ? input.body_a.inverse_mass : 0.0f;
    const float inverse_mass_b = active_b ? input.body_b.inverse_mass : 0.0f;
    const RetailPackedWorldInverseInertia inertia_a = active_a ? input.body_a.world_inverse_inertia : zero_inertia();
    const RetailPackedWorldInverseInertia inertia_b = active_b ? input.body_b.world_inverse_inertia : zero_inertia();

    std::array<float, 3> linear_inverse_effective_mass{};
    for (std::size_t index = 0; index < 3; ++index) {
        const Vector3 a = cross(arm_a, linear_axes[index]);
        const Vector3 b = cross(arm_b, linear_axes[index]);
        const Vector3 ia = constraint_frames::multiply_inertia_from_zero(inertia_a, a);
        const Vector3 ib = constraint_frames::multiply_inertia_from_zero(inertia_b, b);
        // 82AE4814..4990 combines the two bodies component-wise, then uses
        // (Z + Y) + (X + inverse_mass_A + inverse_mass_B), not two dot sums.
        const float x = rs::mul_add(b.x, ib.x, rs::mul_add(a.x, ia.x, 0.0f));
        const float y = rs::mul_add(b.y, ib.y, rs::mul_add(a.y, ia.y, 0.0f));
        const float z = rs::mul_add(b.z, ib.z, rs::mul_add(a.z, ia.z, 0.0f));
        const float denominator = (z + y) + (x + (inverse_mass_a + inverse_mass_b));
        linear_inverse_effective_mass[index] = native_arithmetic::reciprocal_estimate(denominator);
    }
    std::array<float, 3> angular_inverse_effective_mass{};
    for (std::size_t index = 0; index < 3; ++index) {
        const Vector3 axis = angular_axes[index];
        const Vector3 response = add(constraint_frames::multiply_inertia_from_zero(inertia_a, axis),
                                     constraint_frames::multiply_inertia_from_zero(inertia_b, axis));
        const float x = rs::mul_add(axis.x, response.x, 0.0f);
        const float y = rs::mul_add(axis.y, response.y, 0.0f);
        const float z = rs::mul_add(axis.z, response.z, 0.0f);
        // 82AE496C..49A0: Z + (X + Y), one estimate, then the RQD factor 1/2.
        angular_inverse_effective_mass[index] = 0.5f * native_arithmetic::reciprocal_estimate(z + (x + y));
    }

    const Vector3 point_velocity_a = point_rate(input.body_a.linear_velocity, input.body_a.angular_velocity, arm_a);
    const Vector3 point_velocity_b = point_rate(input.body_b.linear_velocity, input.body_b.angular_velocity, arm_b);
    // Native prediction is computed before the active-body inertia mask.
    // An inactive body can still supply the source-owned kinematic rates.
    const Vector3 point_acceleration_a =
        point_rate(input.body_a.force_acceleration, input.body_a.torque_acceleration, arm_a);
    const Vector3 point_acceleration_b =
        point_rate(input.body_b.force_acceleration, input.body_b.torque_acceleration, arm_b);
    const Vector3 displacement_a = scale(
        constraint_frames::multiply_add(point_acceleration_a, input.time_step, point_velocity_a), input.time_step);
    const Vector3 displacement_b = scale(
        constraint_frames::multiply_add(point_acceleration_b, input.time_step, point_velocity_b), input.time_step);
    const Vector3 relative_displacement = sub(displacement_b, displacement_a);
    const Vector3 predicted_separation = sub(add(add(input.body_b.center_of_mass, arm_b), displacement_b),
                                             add(add(input.body_a.center_of_mass, arm_a), displacement_a));
    const auto local_rate_displacement = project(relative_displacement, linear_axes);
    const auto local_separation = project(predicted_separation, linear_axes);

    std::array<float, 3> linear_low{};
    std::array<float, 3> linear_high{};
    const auto position_allowance = vector_to_array(parameters.linear_position_allowance);
    const auto velocity_allowance = vector_to_array(parameters.linear_velocity_allowance);
    for (std::size_t lane = 0; lane < 3; ++lane) {
        const float displacement_low = local_separation[lane] - position_allowance[lane];
        const float displacement_high = local_separation[lane] + position_allowance[lane];
        const float velocity_low = local_rate_displacement[lane] - velocity_allowance[lane] * input.time_step;
        const float velocity_high = local_rate_displacement[lane] + velocity_allowance[lane] * input.time_step;
        const float raw_low = vmx_max(displacement_low, vmx_min(velocity_low, displacement_high));
        const float raw_high = vmx_min(displacement_high, vmx_max(velocity_high, displacement_low));
        linear_low[lane] = raw_low * linear_inverse_effective_mass[lane];
        linear_high[lane] = raw_high * linear_inverse_effective_mass[lane];
    }

    const Vector3 relative_angular_rate =
        sub(constraint_frames::multiply_add(input.body_b.torque_acceleration, input.time_step,
                                            sub(input.body_b.angular_velocity, input.body_a.angular_velocity)),
            scale(input.body_a.torque_acceleration, input.time_step));
    const auto local_angular_displacement = project(scale(relative_angular_rate, input.time_step), angular_axes);
    const std::array<float, 3> angular_velocity_allowance{
        parameters.twist_velocity_allowance,
        parameters.swing_velocity_allowance,
        parameters.swing_velocity_allowance,
    };
    std::array<float, 3> angular_low{};
    std::array<float, 3> angular_high{};
    for (std::size_t lane = 0; lane < 3; ++lane) {
        const float predicted_low = angular_raw_low[lane] + local_angular_displacement[lane];
        const float predicted_high = angular_raw_high[lane] + local_angular_displacement[lane];
        const float velocity_low =
            local_angular_displacement[lane] - angular_velocity_allowance[lane] * input.time_step;
        const float velocity_high =
            rs::mul_add(angular_velocity_allowance[lane], input.time_step, local_angular_displacement[lane]);
        const float raw_low = vmx_max(predicted_low, vmx_min(velocity_low, predicted_high));
        const float raw_high = vmx_min(predicted_high, vmx_max(velocity_high, predicted_low));
        angular_low[lane] = raw_low * angular_inverse_effective_mass[lane];
        angular_high[lane] = raw_high * angular_inverse_effective_mass[lane];
    }

    const auto linear_projection = scaled_projection_columns(linear_axes, linear_inverse_effective_mass);
    const auto angular_projection = scaled_projection_columns(angular_axes, angular_inverse_effective_mass);
    const auto inertia_columns_a = packed_inertia_columns(inertia_a, inverse_mass_a);
    const auto inertia_columns_b = packed_inertia_columns(inertia_b, inverse_mass_b);

    RetailJointJacobian output{};
    write_vector(output, 0, {arm_a.x, arm_a.y, arm_a.z, rs::from_bits(input.body_a.reaction_guest_address)});
    write_vector(output, 1, {arm_b.x, arm_b.y, arm_b.z, rs::from_bits(input.body_b.reaction_guest_address)});
    // Vectors 2 and 3 are the cleared linear/angular accumulators.
    write_vector(output, 4, {linear_projection[0][0], linear_projection[0][1], linear_projection[0][2], 0.0f});
    write_vector(output, 5, {angular_projection[0][0], angular_projection[0][1], angular_projection[0][2], 0.0f});
    write_vector(output, 6,
                 {linear_projection[1][0], linear_projection[1][1], linear_projection[1][2], angular_low[0]});
    write_vector(output, 7,
                 {angular_projection[1][0], angular_projection[1][1], angular_projection[1][2], angular_low[1]});
    write_vector(output, 8,
                 {linear_projection[2][0], linear_projection[2][1], linear_projection[2][2], angular_high[0]});
    write_vector(output, 9,
                 {angular_projection[2][0], angular_projection[2][1], angular_projection[2][2], angular_high[1]});
    write_vector(output, 10, {linear_low[0], linear_low[1], linear_low[2], angular_low[2]});
    write_vector(output, 11, {linear_high[0], linear_high[1], linear_high[2], angular_high[2]});
    write_vector(output, 12,
                 {linear_axes[0].x, linear_axes[0].y, linear_axes[0].z, rs::from_bits(input.joint_guest_address)});
    // Packed carry lanes remain unverified pending direct TU3 instruction review.
    write_vector(output, 13, {linear_axes[1].x, linear_axes[1].y, linear_axes[1].z, linear_axes[1].x});
    write_vector(output, 14, {linear_axes[2].x, linear_axes[2].y, linear_axes[2].z, linear_axes[0].y});
    for (std::size_t lane = 0; lane < 3; ++lane) {
        write_vector(output, 15 + lane,
                     {angular_axes[lane].x, angular_axes[lane].y, angular_axes[lane].z, angular_axes[lane].x});
    }
    for (std::size_t lane = 0; lane < 3; ++lane) {
        write_vector(output, 18 + lane, inertia_columns_a[lane]);
        write_vector(output, 21 + lane, inertia_columns_b[lane]);
    }
    return output;
}

}  // namespace s3::physics::joint_builder
