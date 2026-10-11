// Port of skate-core/src/physics/rigid_body.rs.
#include "s3/core/physics/rigid_body.hpp"

namespace s3::physics::rigid_body {

namespace {
Basis3 unpack_world_inverse_inertia(Vector3 full, Vector3 split) {
    return Basis3{{{
        {full.x, full.y, full.z},
        {full.y, split.y, split.z},
        {full.z, split.z, split.x},
    }}};
}
}  // namespace

RetailBodyRateStep integrate_body_rates(RetailBodyRates body, RetailInertiaDynamics inertia,
                                        RetailSimulationStep simulation,
                                        RetailReactionCorrections reactions) {
    std::array<std::uint32_t, 44> words{};
    words[0] = rs::to_bits(body.orientation.x);
    words[1] = rs::to_bits(body.orientation.y);
    words[2] = rs::to_bits(body.orientation.z);
    words[3] = rs::to_bits(body.orientation.w);
    const std::pair<std::size_t, Vector3> vectors[] = {
        {4, body.position},
        {8, body.linear_velocity},
        {12, body.angular_velocity},
        {36, body.force_acceleration},
        {40, body.torque_acceleration},
    };
    for (const auto& [offset, value] : vectors) {
        words[offset] = rs::to_bits(value.x);
        words[offset + 1] = rs::to_bits(value.y);
        words[offset + 2] = rs::to_bits(value.z);
    }
    words[31] = rs::to_bits(inertia.inverse_mass);
    words[39] = rs::to_bits(body.kinetic_energy);
    words[43] = body.cool_down;
    const std::array<std::uint32_t, 10> native_inertia = rs::to_bits(std::array<float, 10>{
        inertia.inverse_tensor.x,
        inertia.inverse_tensor.y,
        inertia.inverse_tensor.z,
        0.0f,
        inertia.inverse_mass,
        inertia.spherical,
        inertia.maximum_linear_velocity,
        inertia.maximum_angular_velocity,
        inertia.linear_drag,
        inertia.angular_drag,
    });
    std::array<std::uint32_t, 16> correction_words{};
    const Vector3 corrections[4] = {
        reactions.linear_displacement,
        reactions.position_displacement,
        reactions.angular_displacement,
        reactions.orientation_displacement,
    };
    for (std::size_t index = 0; index < 4; ++index) {
        correction_words[index * 4] = rs::to_bits(corrections[index].x);
        correction_words[index * 4 + 1] = rs::to_bits(corrections[index].y);
        correction_words[index * 4 + 2] = rs::to_bits(corrections[index].z);
    }
    const DynamicUpdateResult result =
        dynamic_update_packed(words, native_inertia, simulation, correction_words);
    auto vector = [&](std::size_t offset) {
        return Vector3{rs::from_bits(words[offset]), rs::from_bits(words[offset + 1]),
                       rs::from_bits(words[offset + 2])};
    };
    body.orientation = RetailQuaternion{
        rs::from_bits(words[0]),
        rs::from_bits(words[1]),
        rs::from_bits(words[2]),
        rs::from_bits(words[3]),
    };
    body.position = vector(4);
    body.linear_velocity = vector(8);
    body.angular_velocity = vector(12);
    body.force_acceleration = vector(36);
    body.torque_acceleration = vector(40);
    body.kinetic_energy = rs::from_bits(words[39]);
    body.cool_down = words[43];
    for (std::size_t i = 0; i < 3; ++i) {
        for (std::size_t j = 0; j < 3; ++j) {
            body.basis.columns[i][j] = rs::from_bits(words[16 + i * 4 + j]);
        }
    }
    body.world_inverse_inertia = unpack_world_inverse_inertia(vector(28), vector(32));
    return RetailBodyRateStep{
        body,
        Vector3{result.orientation_displacement[0], result.orientation_displacement[1],
                result.orientation_displacement[2]},
        result.linear_speed_squared,
        result.angular_speed_squared,
    };
}

RetailQuaternion integrate_orientation(RetailQuaternion orientation, Vector3 angular_displacement) {
    const auto q = dynamic_update::orientation(
        {orientation.x, orientation.y, orientation.z, orientation.w},
        {angular_displacement.x, angular_displacement.y, angular_displacement.z, 0.0f});
    return RetailQuaternion{q[0], q[1], q[2], q[3]};
}

Basis3 basis_from_quaternion(RetailQuaternion q) {
    const auto columns = dynamic_update::quaternion_basis({q.x, q.y, q.z, q.w});
    Basis3 out;
    for (std::size_t i = 0; i < 3; ++i) {
        out.columns[i] = {columns[i][0], columns[i][1], columns[i][2]};
    }
    return out;
}

Basis3 world_inverse_inertia(Basis3 basis, Vector3 inverse_tensor) {
    std::array<dynamic_update::V, 3> native_basis{};
    for (std::size_t i = 0; i < 3; ++i) {
        native_basis[i] = {basis.columns[i][0], basis.columns[i][1], basis.columns[i][2], 0.0f};
    }
    const std::array<std::uint32_t, 3> tensor =
        rs::to_bits(std::array<float, 3>{inverse_tensor.x, inverse_tensor.y, inverse_tensor.z});
    const auto [full, split] = dynamic_update::inverse_inertia(native_basis, tensor);
    return unpack_world_inverse_inertia(Vector3{full[0], full[1], full[2]},
                                        Vector3{split[0], split[1], split[2]});
}

RetailPackedWorldInverseInertia pack_world_inverse_inertia(Basis3 tensor) {
    return RetailPackedWorldInverseInertia{
        Vector3{tensor.columns[0][0], tensor.columns[1][0], tensor.columns[2][0]},
        Vector3{tensor.columns[2][2], tensor.columns[1][1], tensor.columns[2][1]},
    };
}

Vector3 multiply_packed_world_inverse_inertia(RetailPackedWorldInverseInertia tensor, Vector3 vector) {
    return Vector3{
        tensor.full.x * vector.x + tensor.full.y * vector.y + tensor.full.z * vector.z,
        tensor.full.y * vector.x + tensor.split.y * vector.y + tensor.split.z * vector.z,
        tensor.full.z * vector.x + tensor.split.z * vector.y + tensor.split.x * vector.z,
    };
}

}  // namespace s3::physics::rigid_body
