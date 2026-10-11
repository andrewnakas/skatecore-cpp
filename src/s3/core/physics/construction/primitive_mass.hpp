// Port of skate-core/src/physics/construction/primitive_mass.rs (module physics::mass::primitive_mass; C++ namespace `primitive_mass_mod`
// because the re-exported fn `mass::primitive_mass` would clash with a same-named namespace).
// Forward principal moments and volume from TU3 82AE7228/82AE6B78.
// These quantities precede mass scaling and inverse-inertia construction.
#pragma once
#include <optional>
#include <variant>

#include "s3/core/math.hpp"

namespace s3::physics::mass::primitive_mass_mod {

using math::Vector3;

/// Rust `enum MassShape`; each variant is a nested struct held in `value`.
struct MassShape {
    struct Sphere {
        float radius = 0.0f;
    };
    /// The capsule's longitudinal principal axis is local Z.
    struct Capsule {
        float radius = 0.0f;
        float half_length = 0.0f;
    };
    struct RoundedBox {
        Vector3 half_extents;
        float radius = 0.0f;
    };
    struct Cylinder {
        float radius = 0.0f;
        float half_length = 0.0f;
        float padding = 0.0f;
    };
    /// Triangle, aggregate, or another type rejected by 82AE7228. The caller
    /// must use the aggregate calculation; no moments are produced here.
    struct Unsupported {};

    std::variant<Sphere, Capsule, RoundedBox, Cylinder, Unsupported> value;

    MassShape() : value(Unsupported{}) {}
    MassShape(Sphere v) : value(v) {}
    MassShape(Capsule v) : value(v) {}
    MassShape(RoundedBox v) : value(v) {}
    MassShape(Cylinder v) : value(v) {}
    MassShape(Unsupported v) : value(v) {}
};
S3_DERIVE_EQ(MassShape::Sphere, radius)
S3_DERIVE_EQ(MassShape::Capsule, radius, half_length)
S3_DERIVE_EQ(MassShape::RoundedBox, half_extents, radius)
S3_DERIVE_EQ(MassShape::Cylinder, radius, half_length, padding)
S3_DERIVE_EQ_EMPTY(MassShape::Unsupported)
S3_DERIVE_EQ(MassShape, value)

struct PrimitiveMass {
    Vector3 moments_per_unit_mass;
    float volume = 0.0f;
};
S3_DERIVE_EQ(PrimitiveMass, moments_per_unit_mass, volume)

/// Complete shape dispatch in 82AE7228. Unsupported shapes return no result,
/// matching the native false return and absence of output writes. All supported
/// branches retain the native scalar operation order and fused multiply-adds.
std::optional<PrimitiveMass> primitive_mass(MassShape shape);

}  // namespace s3::physics::mass::primitive_mass_mod
