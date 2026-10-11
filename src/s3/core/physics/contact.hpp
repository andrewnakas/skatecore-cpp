// Port of skate-core/src/physics/contact.rs.
//
// Skate 3 TU3's RenderWare contact-record construction.
//
// This is the scalar form of the VMX128 block in TU3's two recovered
// add-contact paths. It builds the 256-byte logical `rw::physics::Contact`
// payload consumed in place by `ContactBatchBuild`; it does not solve the
// contact or move a body.
#pragma once
#include <cstdint>
#include <utility>

#include "s3/core/math.hpp"
#include "s3/core/physics/f32_bits.hpp"

namespace s3::physics::contact {

using math::Vector3;

namespace tu3 {
/// Game-side physical-material combine helper used before both recovered
/// collision-to-contact record writers.
inline constexpr std::uint32_t COMBINE_PHYSICAL_MATERIALS = 0x82763078u;
/// First recovered collision-to-contact record block, end exclusive.
inline constexpr std::uint32_t GENERATE_CONTACT_A = 0x8277A828u;
inline constexpr std::uint32_t GENERATE_CONTACT_A_END = 0x8277AA88u;
/// Second field-for-field collision-to-contact record block, end exclusive.
inline constexpr std::uint32_t GENERATE_CONTACT_B = 0x8277BFACu;
inline constexpr std::uint32_t GENERATE_CONTACT_B_END = 0x8277C21Cu;
/// In-place contact Jacobian builder.
inline constexpr std::uint32_t CONTACT_BATCH_BUILD = 0x82AE10C8u;
}  // namespace tu3

/// Exact TU3 constant used to reject a degenerate velocity-derived tangent.
///
/// The Xbox data table at `0x830382B0` contains `0x00800000` in every lane.
/// This is the smallest positive normal binary32 value, not a tuned gameplay
/// threshold.
inline constexpr float CONTACT_TANGENT_MINIMUM_SQUARED = f32_from_bits(0x00800000u);
inline constexpr float CONTACT_FALLBACK_HALF = f32_from_bits(0x3F000000u);

/// The five 16-byte body vectors copied into a contact's solver workspace.
///
/// Original member names and offsets are independently present in preserved
/// Skate-era EA SDK DWARF and are confirmed by TU3's loads:
///
/// - `mCom/mId` at body `+0x10`;
/// - `mIfull/mInvm` at `+0x70`;
/// - `mIsplt/mState` at `+0x80`;
/// - `mForce/mKine` at `+0x90`;
/// - `mTorque/mCool` at `+0xa0`.
struct RetailContactBodyState {
    /// ID written to the contact header from the collision queue's body-ref
    /// entry. It is kept separate from `reaction_id` because TU3 loads them
    /// from different records.
    std::uint32_t contact_body_id = 0;
    Vector3 center_of_mass;
    std::uint32_t reaction_id = 0;
    /// Retail packed world inverse-inertia vector
    /// `mIfull = (Ixx, Ixy, Ixz)`.
    Vector3 inverse_inertia_full;
    float inverse_mass = 0.0f;
    /// Retail packed world inverse-inertia vector
    /// `mIsplt = (Izz, Iyy, Iyz)`.
    Vector3 inverse_inertia_split;
    std::uint32_t state = 0;
    Vector3 force_acceleration;
    float kinetic_energy = 0.0f;
    Vector3 torque_acceleration;
    std::uint32_t cool_down = 0;
    Vector3 linear_velocity;
    Vector3 angular_velocity;
};
S3_DERIVE_EQ(RetailContactBodyState, contact_body_id, center_of_mass, reaction_id,
             inverse_inertia_full, inverse_mass, inverse_inertia_split, state, force_acceleration,
             kinetic_energy, torque_acceleration, cool_down, linear_velocity, angular_velocity)

/// Collision result and already-combined material values consumed by TU3's
/// contact generator.
///
/// The material-combine routine that produces the three scalars is outside the
/// recovered block and remains deliberately explicit at this boundary.
struct RetailContactInput {
    Vector3 position_on_a;
    Vector3 position_on_b;
    /// Positive normal impulse acts along this vector on A and opposite it on
    /// B. For wheel A over static ground B, this points upward. Collision query
    /// adapters must preserve this solver convention when ordering the pair.
    Vector3 normal;
    float restitution = 0.0f;
    float static_friction = 0.0f;
    float dynamic_friction = 0.0f;
    std::uint32_t tag = 0;
};
S3_DERIVE_EQ(RetailContactInput, position_on_a, position_on_b, normal, restitution,
             static_friction, dynamic_friction, tag)

/// Three scalar fields in the game-side physical material record consumed by
/// TU3 `0x82763078`.
struct RetailContactMaterial {
    float static_friction = 0.0f;
    float dynamic_friction = 0.0f;
    float restitution = 0.0f;
};
S3_DERIVE_EQ(RetailContactMaterial, static_friction, dynamic_friction, restitution)

/// Exact game-side material combine used immediately before the recovered
/// contact writers.
///
/// TU3 performs three independent ordered comparisons:
///
/// - static friction is the greater operand;
/// - dynamic friction is the greater operand;
/// - restitution is the lesser operand.
///
/// The comparisons are kept explicit instead of using `f32::max`/`min`
/// because the retail branch behavior for unordered inputs is operand-order
/// sensitive: equality and unordered comparisons select B for all three fields.
/// Scalar lfs/stfs also quiet a selected signalling NaN while preserving payload.
RetailContactMaterial combine_contact_materials(RetailContactMaterial material_a,
                                                RetailContactMaterial material_b);

struct RetailContactWorkspace {
    Vector3 center_of_mass;
    std::uint32_t reaction_id = 0;
    Vector3 inverse_inertia_full;
    float inverse_mass = 0.0f;
    Vector3 inverse_inertia_split;
    std::uint32_t state = 0;
    Vector3 force_acceleration;
    float kinetic_energy = 0.0f;
    Vector3 torque_acceleration;
    std::uint32_t cool_down = 0;

    /// `impl From<RetailContactBodyState> for RetailContactWorkspace`.
    static RetailContactWorkspace from(const RetailContactBodyState& body) {
        return RetailContactWorkspace{
            body.center_of_mass,     body.reaction_id,        body.inverse_inertia_full,
            body.inverse_mass,       body.inverse_inertia_split, body.state,
            body.force_acceleration, body.kinetic_energy,     body.torque_acceleration,
            body.cool_down,
        };
    }
};
S3_DERIVE_EQ(RetailContactWorkspace, center_of_mass, reaction_id, inverse_inertia_full,
             inverse_mass, inverse_inertia_split, state, force_acceleration, kinetic_energy,
             torque_acceleration, cool_down)

/// Scalar representation of the complete logical 256-byte retail contact.
///
/// `body_a_workspace` and `body_b_workspace` correspond to the ten alternating
/// 16-byte vectors at offsets `0x60..0xff`. Keeping those copied values in the
/// record is important: `ContactBatchBuild` reads them from the contact and
/// does not dereference the live bodies.
struct RetailContact {
    Vector3 position_on_a;
    std::uint32_t body_a_id = 0;
    Vector3 position_on_b;
    std::uint32_t body_b_id = 0;
    Vector3 normal;
    float restitution = 0.0f;
    Vector3 tangent_0;
    float static_friction = 0.0f;
    Vector3 tangent_1;
    float dynamic_friction = 0.0f;
    /// Velocity at B's contact point minus velocity at A's contact point.
    Vector3 relative_velocity;
    std::uint32_t tag = 0;
    RetailContactWorkspace body_a_workspace;
    RetailContactWorkspace body_b_workspace;
};
S3_DERIVE_EQ(RetailContact, position_on_a, body_a_id, position_on_b, body_b_id, normal,
             restitution, tangent_0, static_friction, tangent_1, dynamic_friction,
             relative_velocity, tag, body_a_workspace, body_b_workspace)

/// Port of TU3 `0x8277A828..0x8277AA88` and its duplicate
/// `0x8277BFAC..0x8277C21C`.
///
/// VMX128 operation order is retained at the scalar-expression level:
///
/// 1. compute each contact arm from the copied body center of mass;
/// 2. compute point rates as `linear + angular cross arm`;
/// 3. derive tangent 0 from `relative_velocity cross normal`;
/// 4. if that axis is subnormal, use the exact X/Z projection fallback;
/// 5. derive tangent 1 as `normal cross tangent_0`;
/// 6. copy the ten body-state vectors used by `ContactBatchBuild`.
///
/// The retail input normal is already unit length. This routine intentionally
/// does not renormalize it or repair malformed collision input.
RetailContact generate_contact(RetailContactInput input, RetailContactBodyState body_a,
                               RetailContactBodyState body_b);

// Module-private helpers in Rust (visible to its #[cfg(test)] module).
Vector3 cross(Vector3 left, Vector3 right);
float dot(Vector3 left, Vector3 right);

}  // namespace s3::physics::contact
