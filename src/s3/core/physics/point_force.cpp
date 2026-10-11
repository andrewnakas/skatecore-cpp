// Port of skate-core/src/physics/point_force.rs.
#include "s3/core/physics/point_force.hpp"

namespace s3::physics::point_force {

namespace {
Vector3 multiply_basis(const Basis3& basis, const Vector3& v) {
    // Native multiplies column 0, then fuses column 1, then column 2.
    auto lane = [&](std::size_t i) {
        return rs::mul_add(basis.columns[2][i], v.z,
                           rs::mul_add(basis.columns[1][i], v.y, basis.columns[0][i] * v.x));
    };
    return Vector3{lane(0), lane(1), lane(2)};
}
}  // namespace

RetailForceAccumulator accumulate_point_force(RetailForceAccumulator accumulator,
                                              Vector3 force_world, Vector3 application_point_body,
                                              Basis3 deck_basis, float inverse_mass,
                                              Basis3 world_inverse_inertia) {
    // 0x82C03850: vmulfp; 0x82C03878..0x82C03894: scalar adds.
    accumulator.force_acceleration.x += force_world.x * inverse_mass;
    accumulator.force_acceleration.y += force_world.y * inverse_mass;
    accumulator.force_acceleration.z += force_world.z * inverse_mass;
    const Vector3 arm = multiply_basis(deck_basis, application_point_body);
    // 0x82C0387C / 0x82C038B0 / 0x82C038B4: multiply, fused negative
    // multiply-add, permutation. Fusing the positive term instead differs.
    const Vector3 torque{
        rs::mul_add(-arm.z, force_world.y, arm.y * force_world.z),
        rs::mul_add(-arm.x, force_world.z, arm.z * force_world.x),
        rs::mul_add(-arm.y, force_world.x, arm.x * force_world.y),
    };
    const Vector3 angular = multiply_basis(world_inverse_inertia, torque);
    accumulator.torque_acceleration.x += angular.x;
    accumulator.torque_acceleration.y += angular.y;
    accumulator.torque_acceleration.z += angular.z;
    accumulator.cool_down = 0;
    return accumulator;
}

}  // namespace s3::physics::point_force
