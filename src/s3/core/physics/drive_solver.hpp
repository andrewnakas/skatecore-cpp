// Port of skate-core/src/physics/drive_solver.rs.
//
// Scalar port of Skate 3 TU3's RenderWare drive Jacobian and solve path.
//
// A retail drive is not a generic six-degree-of-freedom Bevy joint. TU3
// builds three linear and three quaternion-component constraint rows into a
// 384-byte workspace, then solves each drive sequentially. This module keeps
// those rows in named scalar fields and preserves the observed soft/hard
// coefficient and impulse-limit equations.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

#include "s3/core/math.hpp"
#include "s3/core/physics/drive_frames.hpp"
#include "s3/core/physics/drive_parameters.hpp"
#include "s3/core/physics/rigid_body.hpp"
#include "s3/core/physics/slice.hpp"

namespace s3::physics::drive_solver {

using drive_frames::RetailDriveFrame;
using drive_frames::RetailDriveFrames;
using drive_parameters::RetailDriveDynamics;
using drive_parameters::RetailDriveParams;
using drive_parameters::RetailDriveType;
using math::Vector3;
using rigid_body::RetailPackedWorldInverseInertia;
using rigid_body::RetailQuaternion;
using rigid_body::RetailReactionCorrections;

namespace tu3 {
inline constexpr std::uint32_t DRIVE_JACOBIAN_BUILD = 0x82AE1AE8u;
inline constexpr std::uint32_t ITERATIVE_CONSTRAINT_SOLVER = 0x82AE27D0u;
}  // namespace tu3

inline constexpr std::uint32_t ACTIVE_BODY = 4;

struct RetailDriveBodyState {
    std::size_t reaction_index = 0;
    std::uint32_t state = 0;
    RetailQuaternion orientation;
    /// Native Body+64/+80/+96, independently read for the anchor transform.
    math::Basis3 basis;
    Vector3 center_of_mass;
    Vector3 linear_velocity;
    Vector3 angular_velocity;
    Vector3 force_acceleration;
    Vector3 torque_acceleration;
    float inverse_mass = 0.0f;
    RetailPackedWorldInverseInertia world_inverse_inertia;
};
S3_DERIVE_EQ(RetailDriveBodyState, reaction_index, state, orientation, basis, center_of_mass,
             linear_velocity, angular_velocity, force_acceleration, torque_acceleration,
             inverse_mass, world_inverse_inertia)

struct RetailDriveRows {
    /// Body paired with `DriveFrames::body_a`.
    RetailDriveBodyState frame_a_body;
    /// Body paired with `DriveFrames::body_b`.
    RetailDriveBodyState frame_b_body;
    Vector3 arm_a;
    Vector3 arm_b;
    /// World-space columns of the frame-B basis.
    std::array<Vector3, 3> linear_axes{};
    /// Normalized world-space quaternion-component Jacobian axes.
    ///
    /// Unlike an ordinary rotation basis, these three axes are generally not
    /// orthogonal.
    std::array<Vector3, 3> angular_axes{};
    std::array<float, 3> linear_inverse_effective_mass{};
    std::array<float, 3> angular_inverse_effective_mass{};
    float linear_softness = 0.0f;
    float angular_softness = 0.0f;
    /// Preconditioned target impulses in constraint-row order.
    std::array<float, 3> linear_target_impulse{};
    std::array<float, 3> angular_target_impulse{};
    std::array<float, 3> linear_maximum_impulse{};
    std::array<float, 3> angular_maximum_impulse{};
    std::array<float, 3> accumulated_linear_impulse{};
    std::array<float, 3> accumulated_angular_impulse{};
};
S3_DERIVE_EQ(RetailDriveRows, frame_a_body, frame_b_body, arm_a, arm_b, linear_axes, angular_axes,
             linear_inverse_effective_mass, angular_inverse_effective_mass, linear_softness,
             angular_softness, linear_target_impulse, angular_target_impulse,
             linear_maximum_impulse, angular_maximum_impulse, accumulated_linear_impulse,
             accumulated_angular_impulse)

/// Runs TU3's drive portion of the sequential iterative constraint solver.
void solve_drive_rows(Slice<RetailDriveRows> drives, Slice<RetailReactionCorrections> reactions,
                      std::uint32_t maximum_iterations);

/// One drive pass through the same native kernel used by the shared solver.
/// Both linear and angular candidates read the initial reactions of this
/// drive, before either candidate is applied (82AE2ED0..82AE2FB0).
void solve_drive_iteration(Slice<RetailDriveRows> drives, Slice<RetailReactionCorrections> reactions);

// Module-private helpers shared with the `builder` child module.
bool is_active(const RetailDriveBodyState& body);
std::array<Vector3, 3> basis_columns(const math::Basis3& basis);
std::array<float, 3> project(Vector3 vector, const std::array<Vector3, 3>& axes);
Vector3 point_rate(Vector3 linear, Vector3 angular, Vector3 arm);
constexpr Vector3 add(Vector3 a, Vector3 b) { return Vector3{a.x + b.x, a.y + b.y, a.z + b.z}; }
constexpr Vector3 sub(Vector3 a, Vector3 b) { return Vector3{a.x - b.x, a.y - b.y, a.z - b.z}; }
constexpr Vector3 scale(Vector3 value, float factor) {
    return Vector3{value.x * factor, value.y * factor, value.z * factor};
}
float dot(Vector3 a, Vector3 b);
Vector3 cross(Vector3 a, Vector3 b);

}  // namespace s3::physics::drive_solver

#include "s3/core/physics/drive_solver/builder.hpp"

namespace s3::physics::drive_solver {
using builder::build_drive_rows;
}  // namespace s3::physics::drive_solver
