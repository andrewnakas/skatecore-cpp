// Port of skate-core/src/physics/rigid_body.rs.
//
// TU3 RenderWare rigid-body accumulation and integration primitives.
//
// These routines are ports of the arithmetic in the Skate 3 TU3 executable,
// not a wrapper around a generic rigid-body engine. The caller supplies
// solver corrections; this module does not own collision or solver scheduling.
#pragma once
#include <cstdint>

#include "s3/core/math.hpp"
#include "s3/core/physics/point_force.hpp"

namespace s3::physics::rigid_body {

using math::Basis3;
using math::Vector3;
using point_force::accumulate_point_force;
using point_force::RetailForceAccumulator;

namespace tu3 {
/// Per-active-body integration routine called by
/// `rw::physics::Simulation::BatchIntegrator`.
inline constexpr std::uint32_t BATCH_INTEGRATOR_BODY = 0x82AE6590u;
/// `Sk8::Physics::Skateboard::ApplyQueuedSkateboardForces`.
inline constexpr std::uint32_t APPLY_QUEUED_SKATEBOARD_FORCES = 0x82C03718u;
}  // namespace tu3

/// Exact scalar fields read from TU3's `rw::physics::Simulation` by `0x82AE6590`.
struct RetailSimulationStep {
    float time_step = 0.0f;
    float frequency = 0.0f;
    std::uint32_t cool_down = 0;
    float minimum_energy = 0.0f;
    Vector3 gravity_acceleration;

    static RetailSimulationStep fixed_60_hz(std::uint32_t cool_down, float minimum_energy,
                                            Vector3 gravity_acceleration) {
        const float time_step = rs::from_bits(0x3C888889u);
        return RetailSimulationStep{time_step, rs::recip(time_step), cool_down, minimum_energy,
                                    gravity_acceleration};
    }
};
S3_DERIVE_EQ(RetailSimulationStep, time_step, frequency, cool_down, minimum_energy,
             gravity_acceleration)

/// Exact scalar layout of `rw::physics::Inertia` after its inverse-tensor vector.
struct RetailInertiaDynamics {
    Vector3 inverse_tensor;
    float inverse_mass = 0.0f;
    float spherical = 0.0f;
    float maximum_linear_velocity = 0.0f;
    float maximum_angular_velocity = 0.0f;
    float linear_drag = 0.0f;
    float angular_drag = 0.0f;
};
S3_DERIVE_EQ(RetailInertiaDynamics, inverse_tensor, inverse_mass, spherical,
             maximum_linear_velocity, maximum_angular_velocity, linear_drag, angular_drag)

/// Inverse local principal-axis frame stored by TU3 `ComputeMassProperties`
/// (`0x82AE7770`) at `PartDefinition + 64` (`inverseBodyLTM`).
///
/// The stock wheel and truck retain identity. Deck construction `0x82C09290`
/// also explicitly resets this frame to identity after computing aggregate
/// principal inertia; that override does not discard the calculated inertia.
struct RetailLocalMassFrame {
    /// Column vectors in RenderWare `Ri`, `Up`, `At` order.
    Basis3 basis;
    Vector3 translation;

    static const RetailLocalMassFrame IDENTITY;
};
inline constexpr RetailLocalMassFrame RetailLocalMassFrame::IDENTITY = RetailLocalMassFrame{
    Basis3{{{{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}}},
    Vector3{0.0f, 0.0f, 0.0f},
};
S3_DERIVE_EQ(RetailLocalMassFrame, basis, translation)

/// Complete per-part mass data needed by the TU3 rigid-body integrator.
struct RetailBodyMassProperties {
    RetailLocalMassFrame local_mass_frame;
    RetailInertiaDynamics dynamics;
};
S3_DERIVE_EQ(RetailBodyMassProperties, local_mass_frame, dynamics)

/// The four 16-byte correction vectors indexed by `RigidBody::mId`.
///
/// The first and third vectors participate in the velocity-producing frame
/// displacement. The second and fourth vectors correct position and
/// orientation only. This separation is directly visible in `0x82AE6590`.
struct RetailReactionCorrections {
    Vector3 linear_displacement;
    Vector3 position_displacement;
    Vector3 angular_displacement;
    Vector3 orientation_displacement;
};
S3_DERIVE_EQ(RetailReactionCorrections, linear_displacement, position_displacement,
             angular_displacement, orientation_displacement)

/// RenderWare quaternion lane order is `(x, y, z, w)`.
struct RetailQuaternion {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;

    static const RetailQuaternion IDENTITY;
};
inline constexpr RetailQuaternion RetailQuaternion::IDENTITY = RetailQuaternion{0.0f, 0.0f, 0.0f, 1.0f};
S3_DERIVE_EQ(RetailQuaternion, x, y, z, w)
inline std::ostream& operator<<(std::ostream& os, const RetailQuaternion& q) {
    return os << "RetailQuaternion { x: " << q.x << ", y: " << q.y << ", z: " << q.z
              << ", w: " << q.w << " }";
}

struct RetailBodyRates {
    RetailQuaternion orientation;
    Basis3 basis;
    Basis3 world_inverse_inertia;
    Vector3 position;
    Vector3 linear_velocity;
    Vector3 angular_velocity;
    /// Acceleration accumulator. `RigidBody::AddForce` has already multiplied
    /// physical force by inverse mass before this field reaches the integrator.
    Vector3 force_acceleration;
    /// Angular-acceleration accumulator. Point-force torque has already passed
    /// through the body's world inverse inertia.
    Vector3 torque_acceleration;
    float kinetic_energy = 0.0f;
    std::uint32_t cool_down = 0;
};
S3_DERIVE_EQ(RetailBodyRates, orientation, basis, world_inverse_inertia, position, linear_velocity,
             angular_velocity, force_acceleration, torque_acceleration, kinetic_energy, cool_down)

struct RetailBodyRateStep {
    RetailBodyRates state;
    /// Rotation increment consumed by the quaternion branch in the same retail
    /// function. Keeping this explicit prevents a presentation system from
    /// replacing it with a surface-normal snap.
    Vector3 orientation_displacement;
    float linear_speed_squared = 0.0f;
    float angular_speed_squared = 0.0f;
};
S3_DERIVE_EQ(RetailBodyRateStep, state, orientation_displacement, linear_speed_squared,
             angular_speed_squared)

/// Skate-era RenderWare's six-value packed symmetric world inverse-inertia tensor.
///
/// The lane mapping is:
///
/// - `mIfull = (Ixx, Ixy, Ixz)`;
/// - `mIsplt = (Izz, Iyy, Iyz)`.
///
/// Independently traced through original S3 DynamicUpdate82AE6778..6804 and
/// S2 DynamicUpdate82AE467C..46FC. This verifies the six-component layout,
/// not bit-exact Xenon arithmetic; generated-code validation is withdrawn.
struct RetailPackedWorldInverseInertia {
    Vector3 full;
    Vector3 split;
};
S3_DERIVE_EQ(RetailPackedWorldInverseInertia, full, split)

/// Typed adapter to the complete TU3 `RigidBody::DynamicUpdate` (`0x82AE6590`).
/// The packed entry point additionally preserves guest metadata and clears its
/// mutable reaction record. This value-taking adapter returns the updated body.
///
/// The order matters:
///
/// 1. acceleration is integrated to a candidate velocity;
/// 2. candidate velocity is converted to a frame displacement;
/// 3. solver correction displacements are added;
/// 4. position is advanced;
/// 5. velocity is reconstructed from displacement with retail drag;
/// 6. linear and angular speed caps are applied;
/// 7. energy/cool-down is updated;
/// 8. force/torque accumulators are reset for the next step.
///
/// `BoardStep` supplies all four correction vectors after the shared contact,
/// joint and drive solve. Player-state scheduling remains a separate owner.
RetailBodyRateStep integrate_body_rates(RetailBodyRates body, RetailInertiaDynamics inertia,
                                        RetailSimulationStep simulation,
                                        RetailReactionCorrections reactions);

/// Quaternion branch from TU3 `0x82AE6668..0x82AE66C8`, paired with
/// Skate 2 `0x82AE455C..0x82AE45C8`.
///
/// `q += 0.5 * Quaternion(angular_displacement, 0) * q` followed by normalization.
/// This pre-multiplication order is important: swapping it changes the board's
/// world-space angular response.
RetailQuaternion integrate_orientation(RetailQuaternion orientation, Vector3 angular_displacement);

/// `Ri`, `Up`, and `At` columns rebuilt immediately after TU3 normalizes the
/// quaternion in `0x82AE6590`.
Basis3 basis_from_quaternion(RetailQuaternion q);

/// Native packed inverse-inertia rebuild in 82AE6590, with its fused order.
Basis3 world_inverse_inertia(Basis3 basis, Vector3 inverse_tensor);

/// Packs a symmetric world inverse-inertia tensor in RenderWare's `mIfull` /
/// `mIsplt` lane order.
RetailPackedWorldInverseInertia pack_world_inverse_inertia(Basis3 tensor);

/// Multiplies a vector by the exact symmetric tensor represented by retail
/// `mIfull` and `mIsplt`.
Vector3 multiply_packed_world_inverse_inertia(RetailPackedWorldInverseInertia tensor, Vector3 vector);

}  // namespace s3::physics::rigid_body

// Re-exports (`pub use dynamic_update::{...}`) live in the child header.
#include "s3/core/physics/rigid_body/dynamic_update.hpp"
