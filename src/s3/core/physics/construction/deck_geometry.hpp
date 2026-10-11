// Port of skate-core/src/physics/construction/deck_geometry.rs (module physics::mass::deck_geometry).
// Deck collision children constructed by TU3 82C09290 and 82C0A2A0/3E0/510.
// Allocation is host-owned; dimensions, child order, flags and arithmetic are
// source-derived. Mass uses the actual primitive/triangle-AABB dispatch in
// 82AE7058, including children whose collision-enable bit is clear.
//
// Header-cycle note: Rust drive_frames uses mass::retail_deck_mass_properties and this
// module uses drive_frames::RetailAffineTransform. drive_frames.hpp must therefore not
// require mass.hpp in its declarations (include mass.hpp only from drive_frames.cpp).
#pragma once
#include <array>
#include <cstdint>
#include <variant>
#include <vector>

#include "s3/core/math.hpp"
#include "s3/core/physics/construction/mass_moments.hpp"
#include "s3/core/physics/construction/primitive_mass.hpp"
#include "s3/core/physics/drive_frames.hpp"

namespace s3::physics::mass::deck_geometry {

using drive_frames::RetailAffineTransform;
using mass_moments::MassMoments;
using math::Vector3;

struct DeckGeometrySettings {
    float width = 0.0f;
    float mid_length = 0.0f;
    float thickness = 0.0f;
    /// Both fans read DeckBackEndSize. DeckFrontEndSize is not read here.
    float back_end_size = 0.0f;
    float front_end_angle_degrees = 0.0f;
    float back_end_angle_degrees = 0.0f;
    /// DeckEndCapsules (Int32), despite the triangles produced by this loop.
    std::int32_t end_capsule_count = 0;
    bool enable_deck_volume_collisions = false;
    bool enable_end_volume_collisions = false;

    /// physicsdeck/default.xml, retaining the original Float words.
    static const DeckGeometrySettings STOCK;
};
inline const DeckGeometrySettings DeckGeometrySettings::STOCK = DeckGeometrySettings{
    rs::from_bits(0x3E75C28Fu),
    rs::from_bits(0x3F170A3Du),
    rs::from_bits(0x3C75C28Fu),
    rs::from_bits(0x3E28F5C3u),
    rs::from_bits(0x41500000u),
    rs::from_bits(0x41480000u),
    5,
    true,
    true,
};
S3_DERIVE_EQ(DeckGeometrySettings, width, mid_length, thickness, back_end_size,
             front_end_angle_degrees, back_end_angle_degrees, end_capsule_count,
             enable_deck_volume_collisions, enable_end_volume_collisions)

/// Rust `enum DeckShape`; each variant is a nested struct held in `value`.
struct DeckShape {
    struct RoundedBox {
        Vector3 half_extents;
        float radius = 0.0f;
    };
    struct Capsule {
        float radius = 0.0f;
        float half_length = 0.0f;
    };
    struct Sphere {
        float radius = 0.0f;
    };
    struct Triangle {
        std::array<Vector3, 3> vertices{};
        float fatness = 0.0f;
        std::array<float, 3> edge_cosines{};
        /// Volume flags excluding the separately exposed collision-enable bit.
        /// These are Volume flags, not GPTriangle feature flags.
        std::uint32_t volume_flags = 0;
    };

    std::variant<RoundedBox, Capsule, Sphere, Triangle> value;

    DeckShape() : value(RoundedBox{}) {}
    DeckShape(RoundedBox v) : value(v) {}
    DeckShape(Capsule v) : value(v) {}
    DeckShape(Sphere v) : value(v) {}
    DeckShape(Triangle v) : value(v) {}
};
S3_DERIVE_EQ(DeckShape::RoundedBox, half_extents, radius)
S3_DERIVE_EQ(DeckShape::Capsule, radius, half_length)
S3_DERIVE_EQ(DeckShape::Sphere, radius)
S3_DERIVE_EQ(DeckShape::Triangle, vertices, fatness, edge_cosines, volume_flags)
S3_DERIVE_EQ(DeckShape, value)

struct DeckChild {
    DeckShape shape;
    RetailAffineTransform transform;
    bool collision_enabled = false;

    MassMoments mass_moments() const;
};
S3_DERIVE_EQ(DeckChild, shape, transform, collision_enabled)

struct DeckGeometry {
    std::vector<DeckChild> children;

    static DeckGeometry new_(DeckGeometrySettings settings);
    MassMoments mass_moments() const;
};
S3_DERIVE_EQ(DeckGeometry, children)

}  // namespace s3::physics::mass::deck_geometry
