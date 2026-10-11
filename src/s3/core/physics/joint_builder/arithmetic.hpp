// Port of skate-core/src/physics/joint_builder/arithmetic.rs.
// Original TU3 joint angular limits and shared constraint-frame arithmetic.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <tuple>

#include "s3/core/physics/joint_builder.hpp"

namespace s3::physics::joint_builder::arithmetic {

JointParameters decode_parameters(RetailJointParametersRaw raw);
JointFrames decode_frames(RetailJointFramesRaw raw);
std::tuple<std::array<Vector3, 3>, std::array<float, 3>, std::array<float, 3>> angular_rows(
    JointParameters parameters, Basis3 basis_a, Basis3 basis_b,
    std::array<std::array<float, 3>, 3> relative, Rqd rqd);
Rqd create_rqd(RetailQuaternion world_a, RetailQuaternion world_b);
std::array<std::array<float, 3>, 3> relative_basis(Basis3 a, Basis3 b);
std::array<std::array<float, 3>, 3> scaled_projection_columns(std::array<Vector3, 3> axes,
                                                              std::array<float, 3> inverse_effective_mass);
std::array<std::array<float, 4>, 3> packed_inertia_columns(RetailPackedWorldInverseInertia packed,
                                                           float inverse_mass);
RetailPackedWorldInverseInertia zero_inertia();
Basis3 basis_from_quaternion_retail(RetailQuaternion q);
RetailQuaternion quaternion_multiply(RetailQuaternion a, RetailQuaternion b);
Vector3 point_rate(Vector3 linear, Vector3 angular, Vector3 arm);
std::array<float, 3> project(Vector3 vector, std::array<Vector3, 3> axes);
Vector3 multiply_basis(Basis3 basis, Vector3 vector);
std::array<Vector3, 3> basis_columns(Basis3 basis);
Vector3 basis_column(Basis3 basis, std::size_t index);
template <std::size_t N>
Vector3 vector_from_words(const std::array<std::uint32_t, N>& words, std::size_t offset) {
    return Vector3{rs::from_bits(words[offset]), rs::from_bits(words[offset + 1]),
                   rs::from_bits(words[offset + 2])};
}
template <std::size_t N>
RetailQuaternion quaternion_from_words(const std::array<std::uint32_t, N>& words, std::size_t offset) {
    return RetailQuaternion{rs::from_bits(words[offset]), rs::from_bits(words[offset + 1]),
                            rs::from_bits(words[offset + 2]), rs::from_bits(words[offset + 3])};
}
std::array<float, 3> vector_to_array(Vector3 value);
Vector3 cross(Vector3 left, Vector3 right);
float dot3(Vector3 left, Vector3 right);
Vector3 add(Vector3 left, Vector3 right);
Vector3 sub(Vector3 left, Vector3 right);
Vector3 scale(Vector3 value, float scalar);
float vmx_min(float left, float right);
float vmx_max(float left, float right);

}  // namespace s3::physics::joint_builder::arithmetic

// `use arithmetic::*` in the parent module.
namespace s3::physics::joint_builder {
using namespace arithmetic;
}  // namespace s3::physics::joint_builder
