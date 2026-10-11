// Port of skate-core/src/physics/construction/board_parts.rs (module physics::mass::board_parts).
// Wheel/truck mass-input producers in TU3 82C0AA78 and 82C0A6F8.
// Allocation, material publication and final inverse inertia are separate stages.
#pragma once
#include <optional>

#include "s3/core/physics/construction/primitive_mass.hpp"

namespace s3::physics::mass::board_parts {

using math::Vector3;
using primitive_mass_mod::MassShape;
using primitive_mass_mod::PrimitiveMass;

struct WheelMassSettings {
    /// physicswheels.WheelRadius; 82C0AB44..82C0AB8C.
    float radius = 0.0f;
    /// physicswheels.WheelMass; 82C0ACC8..82C0AD04.
    float mass = 0.0f;
    /// physics_world.SkateboardMassFactor; 82C0AD0C..82C0AD58.
    float mass_factor = 0.0f;

    /// Decoded stock collection inputs, not cached mass-property outputs.
    static const WheelMassSettings STOCK;
};
inline const WheelMassSettings WheelMassSettings::STOCK = WheelMassSettings{
    rs::from_bits(0x3cfdf3b6u),
    rs::from_bits(0x3da9fbe7u),
    rs::from_bits(0x40a00000u),
};
S3_DERIVE_EQ(WheelMassSettings, radius, mass, mass_factor)

struct TruckMassSettings {
    float wheel_radius = 0.0f;
    float wheel_x_distance = 0.0f;
    /// physicstrucks layout +52 and +56.
    float radius_scalar = 0.0f;
    float half_height_scalar = 0.0f;
    /// physicstrucks layout +40 and physics_world.SkateboardMassFactor.
    float mass = 0.0f;
    float mass_factor = 0.0f;

    static const TruckMassSettings STOCK;
};
inline const TruckMassSettings TruckMassSettings::STOCK = TruckMassSettings{
    rs::from_bits(0x3cfdf3b6u), rs::from_bits(0x3dc28f5cu), rs::from_bits(0x3f09999au),
    rs::from_bits(0x3faccccdu), rs::from_bits(0x3ec7ae14u), rs::from_bits(0x40a00000u),
};
S3_DERIVE_EQ(TruckMassSettings, wheel_radius, wheel_x_distance, radius_scalar, half_height_scalar,
             mass, mass_factor)

/// Actual shape dimensions and requested mass passed to 82AE7770. Its inverse
/// principal moments still require the unresolved native reciprocal estimate.
struct PartMassInput {
    MassShape shape;
    float requested_mass = 0.0f;
};
S3_DERIVE_EQ(PartMassInput, shape, requested_mass)

/// 82C0AA78 creates one shared sphere and applies the same mass inputs to all
/// four wheel definitions. WheelCapsuleLength is not read by this function.
PartMassInput wheel_mass_input(WheelMassSettings settings);

/// 82C0A6F8 -> 82AD9708: capsule radius and half-length each include the final
/// multiplication by one half. Both truck definitions share this shape.
PartMassInput truck_mass_input(TruckMassSettings settings);

struct ForwardMassProperties {
    PrimitiveMass primitive;
    float mass = 0.0f;
    Vector3 principal_moments;
};
S3_DERIVE_EQ(ForwardMassProperties, primitive, mass, principal_moments)

/// The supported primitive stage of 82AE7770: shape calculation, density-one
/// mass fallback at 82AE79EC, then principal-moment scaling at 82AE7A24.
/// An unsupported shape needs the aggregate producer and returns no result.
/// This does not supply inverse inertia or claim the part is ready to simulate.
std::optional<ForwardMassProperties> forward_mass_properties(PartMassInput input);

}  // namespace s3::physics::mass::board_parts
