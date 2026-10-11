// Port of skate-core/src/physics/world_contact/primitive_geometry.rs
// (module physics::world_contact::primitive_query::geometry via #[path]).
#pragma once
#include <utility>
#include <variant>

#include "s3/core/math.hpp"
#include "s3/core/physics/collision.hpp"
#include "s3/core/physics/world_contact/projection.hpp"

namespace s3::physics::world_contact::primitive_query::geometry {

using collision::Sphere;
using collision::Triangle;
using collision::TriangleFeature;
using math::Basis3;
using math::Vector3;
using projection::PrimitiveKind;

/// World-space board geometry. Capsule axes and box basis columns are the
/// transformed native volume axes; shape radii are applied after feature fixup.
/// Rust enum { Sphere(Sphere), Capsule{..}, Triangle(Triangle), RoundedBox{..} }.
struct ContactPrimitive {
    struct Capsule {
        Vector3 center;
        Vector3 axis;
        float half_length = 0.0f;
        float radius = 0.0f;
    };
    struct RoundedBox {
        Vector3 center;
        Basis3 basis;
        Vector3 half_extents;
        float radius = 0.0f;
    };
    std::variant<Sphere, Capsule, Triangle, RoundedBox> value;

    ContactPrimitive(Sphere v) : value(v) {}
    ContactPrimitive(Capsule v) : value(v) {}
    ContactPrimitive(Triangle v) : value(v) {}
    ContactPrimitive(RoundedBox v) : value(v) {}
    const Triangle* as_triangle() const { return std::get_if<Triangle>(&value); }
};

/// Authored triangle Volume initialization and CreateGPInstance geometry
/// (82AC75B8, 82ADE468). The normal cache's dirty bit is cleared before copying
/// the Volume flags to GP flags; all other flags and edge cosines copy directly.
/// This constructor recomputes authored geometry. A cached native GPTriangle can
/// instead be passed directly as ContactPrimitive::Triangle.
Triangle triangle_from_volume(std::array<Vector3, 3> vertices, float fatness, std::array<float, 3> edge_cosines,
                              std::uint32_t volume_flags);

/// TriangleVolume::CreateGPInstance with a rigid Volume-to-world transform.
/// Source order computes the local normal first, transforms it without a new
/// normalization, and then builds directions and lengths from world vertices.
Triangle transform_triangle_volume(std::array<Vector3, 3> vertices, float fatness,
                                   std::array<float, 3> edge_cosines, std::uint32_t volume_flags, Basis3 basis,
                                   Vector3 translation);

// pub(in super::super)
std::pair<std::array<std::uint32_t, 48>, PrimitiveKind> pack(const ContactPrimitive& shape);
std::array<float, 4> lanes(Vector3 v, float w);
Vector3 xyz(std::array<float, 4> v);

}  // namespace s3::physics::world_contact::primitive_query::geometry
