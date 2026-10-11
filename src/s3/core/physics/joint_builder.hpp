// Port of skate-core/src/physics/joint_builder.rs.
//
// Semantic port of TU3 `rw::physics::JointJacobian::Build`.
//
// This module is intentionally separate from generic engine joints.  It
// accepts the exact 64-byte parameter and 80-byte frame records emitted by
// `SkateboardBody::CreateJoints`, plus the rigid-body fields read by
// `0x82AE3BC8`, and emits the 384-byte workspace consumed by the joint branch
// of `0x82AE27D0`.
//
// Evidence labels used below:
// - **Observed**: direct field loads, stores, constants, or branches in TU3.
// - **Derived**: scalar algebra preserving the observed VMX data flow.
// - **Inferred**: source-level naming where the executable has no type name.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

#include "s3/core/math.hpp"
#include "s3/core/physics/f32_bits.hpp"
#include "s3/core/physics/joint_records.hpp"
#include "s3/core/physics/joint_solver.hpp"
#include "s3/core/physics/rigid_body.hpp"

namespace s3::physics::joint_builder {

using joint_records::RetailJointFramesRaw;
using joint_records::RetailJointParametersRaw;
using joint_solver::RetailJointJacobian;
using math::Basis3;
using math::Vector3;
using rigid_body::RetailPackedWorldInverseInertia;
using rigid_body::RetailQuaternion;

namespace tu3 {
inline constexpr std::uint32_t JOINT_BATCH_BUILD = 0x82AE39D0u;
inline constexpr std::uint32_t JOINT_JACOBIAN_BUILD = 0x82AE3BC8u;
inline constexpr std::uint32_t JACOBIAN_RQD_CREATE = 0x82AE0DB0u;
inline constexpr std::uint32_t ITERATIVE_CONSTRAINT_SOLVER = 0x82AE27D0u;

inline constexpr std::uint32_t ACTIVE_BODY = 4;
inline constexpr std::size_t JACOBIAN_BYTES = 0x180;
inline constexpr std::size_t JACOBIAN_WORDS = JACOBIAN_BYTES / 4;

/// Observed TU3 vector constant used as the finite "unbounded" value.
inline constexpr float FINITE_INFINITY = f32_from_bits(0x7F7FFFFFu);
/// Observed singularity guard initialized at `0x830BDE50`.
inline constexpr float NEAR_PARALLEL = f32_from_bits(0x3F7FFF58u);
}  // namespace tu3

/// Explicit body snapshot consumed by `JointJacobian::Build`.
///
/// All fields are **Observed** inputs.  `basis` is kept separate from
/// `orientation` because TU3 reads both rather than rebuilding one from the
/// other.  Likewise the reaction address is metadata copied into the raw
/// Jacobian; this port never dereferences it.
struct RetailJointBodyInput {
    std::uint32_t reaction_guest_address = 0;
    std::uint32_t state = 0;
    RetailQuaternion orientation;
    Vector3 center_of_mass;
    Basis3 basis;
    Vector3 linear_velocity;
    Vector3 angular_velocity;
    Vector3 force_acceleration;
    Vector3 torque_acceleration;
    float inverse_mass = 0.0f;
    RetailPackedWorldInverseInertia world_inverse_inertia;
};
S3_DERIVE_EQ(RetailJointBodyInput, reaction_guest_address, state, orientation, center_of_mass, basis,
             linear_velocity, angular_velocity, force_acceleration, torque_acceleration, inverse_mass,
             world_inverse_inertia)

/// Complete explicit call state for TU3 `JointJacobian::Build`.
struct RetailJointBuildInput {
    /// **Observed** 64-byte record at `Joint+4`.
    RetailJointParametersRaw parameters;
    /// **Observed** 80-byte record at `Joint+0`.
    RetailJointFramesRaw frames;
    /// **Observed** body at `Joint+0x10`.
    RetailJointBodyInput body_a;
    /// **Observed** body at `Joint+0x14`.
    RetailJointBodyInput body_b;
    /// **Observed** `Simulation+0xA0`.
    float time_step = 0.0f;
    /// **Observed** source `Joint*`, retained in vector 12's fourth word.
    std::uint32_t joint_guest_address = 0;
};
S3_DERIVE_EQ(RetailJointBuildInput, parameters, frames, body_a, body_b, time_step, joint_guest_address)

// Module-private types (visible to the arithmetic child module and tests).
struct JointParameters {
    Vector3 linear_position_allowance;
    Vector3 linear_velocity_allowance;
    float twist_velocity_allowance = 0.0f;
    float swing_velocity_allowance = 0.0f;
    float swing_threshold = 0.0f;
    float twist_threshold = 0.0f;
    std::uint32_t swing_mode = 0;
    std::uint32_t twist_mode = 0;
};
S3_DERIVE_EQ(JointParameters, linear_position_allowance, linear_velocity_allowance,
             twist_velocity_allowance, swing_velocity_allowance, swing_threshold, twist_threshold,
             swing_mode, twist_mode)

struct JointFrames {
    RetailQuaternion orientation_a;
    Vector3 anchor_a;
    RetailQuaternion orientation_b;
    Vector3 anchor_b;
    RetailQuaternion linear_orientation_b;
};
S3_DERIVE_EQ(JointFrames, orientation_a, anchor_a, orientation_b, anchor_b, linear_orientation_b)

struct Rqd {
    RetailQuaternion relative;
    std::array<Vector3, 3> axes{};
};

/// Builds TU3's fixed six-lane, 384-byte joint workspace.
///
/// The three linear and three angular accumulators begin at zero because the
/// retail builder clears all three 128-byte cache lines on every batch build.
RetailJointJacobian build_retail_joint_jacobian(RetailJointBuildInput input);

}  // namespace s3::physics::joint_builder

#include "s3/core/physics/joint_builder/arithmetic.hpp"
