// Port of skate-core/src/physics/construction/primitive_mass.rs.
#include "s3/core/physics/construction/primitive_mass.hpp"

namespace s3::physics::mass::primitive_mass_mod {

namespace {
const float MIN_RADIUS = rs::from_bits(0x33d6bf95u);
const float SPHERE_VOLUME = rs::from_bits(0x40860a92u);
const float FOUR_THIRDS = rs::from_bits(0x3faaaaabu);
const float TWO_THIRDS = rs::from_bits(0x3f2aaaabu);
const float ONE_THIRD = rs::from_bits(0x3eaaaaabu);
const float ONE_TWELFTH = rs::from_bits(0x3daaaaabu);
const float PI = rs::from_bits(0x40490fdbu);
const float TWO_PI = rs::from_bits(0x40c90fdbu);

// The native fsubs/fsel chooses the right operand for an unordered difference.
// Keep this ordered comparison, including its signed-zero operand selection.
float greater_extent(float left, float right) { return left - right >= 0.0f ? left : right; }

/// Complete 82AE6B78: native rounded-box dimension floor, moments and volume.
PrimitiveMass rounded_box_mass(Vector3 half_extents, float radius) {
    const float largest =
        greater_extent(half_extents.x, greater_extent(half_extents.y, half_extents.z));
    const float floor = greater_extent((largest + radius) * 0.05f, MIN_RADIUS) - radius;
    const float x = greater_extent(half_extents.x, floor);
    const float y = greater_extent(half_extents.y, floor);
    const float z = greater_extent(half_extents.z, floor);
    const float radius_moment = (radius * radius) * 0.5f;
    const float x_squared = x * x;
    const float y_squared = y * y;
    const float z_squared = z * z;
    const float xy_radius = (x + y) * radius;
    const float yz_radius = (y + z) * radius;
    const float xz_radius = (x + z) * radius;
    const float moment_x =
        rs::mul_add(rs::mul_add(yz_radius, 2.0f, z_squared) + y_squared, ONE_THIRD, radius_moment);
    const float moment_y =
        rs::mul_add(rs::mul_add(xz_radius, 2.0f, x_squared) + z_squared, ONE_THIRD, radius_moment);
    const float moment_z =
        rs::mul_add(rs::mul_add(xy_radius, 2.0f, x_squared) + y_squared, ONE_THIRD, radius_moment);
    const float rounded_edges = (((rs::mul_add(radius, TWO_THIRDS, x) + y) + z) * radius) * TWO_PI;
    const float face_products = rs::mul_add(y + z, x, y * z);
    const float box_volume = ((x * y) * z) * 8.0f;
    return PrimitiveMass{
        Vector3{moment_x, moment_y, moment_z},
        rs::mul_add(rs::mul_add(face_products, 8.0f, rounded_edges), radius, box_volume),
    };
}

struct Dispatch {
    std::optional<PrimitiveMass> operator()(const MassShape::Unsupported&) const {
        return std::nullopt;
    }
    std::optional<PrimitiveMass> operator()(const MassShape::Sphere& s) const {
        const float radius = greater_extent(s.radius, MIN_RADIUS);
        const float radius_squared = radius * radius;
        const float moment = radius_squared * 0.4f;
        return PrimitiveMass{
            Vector3{moment, moment, moment},
            (radius_squared * radius) * SPHERE_VOLUME,
        };
    }
    std::optional<PrimitiveMass> operator()(const MassShape::Capsule& c) const {
        const float half_length = c.half_length;
        const float length_squared = half_length * half_length;
        const float radius =
            greater_extent(greater_extent(c.radius, MIN_RADIUS), half_length * 0.05f);
        const float numerator = rs::mul_add(
            rs::mul_add(rs::mul_add(radius, 1.6f, half_length * 0.75f), radius,
                        length_squared * 4.0f),
            radius, length_squared * half_length);
        const float denominator = rs::mul_add(radius, 4.0f, half_length * 3.0f);
        const float transverse = numerator / denominator;
        const float axial = (radius * radius) * 0.4f;
        return PrimitiveMass{
            Vector3{transverse, transverse, axial},
            ((rs::mul_add(radius, FOUR_THIRDS, half_length * 2.0f) * radius) * radius) * PI,
        };
    }
    std::optional<PrimitiveMass> operator()(const MassShape::RoundedBox& b) const {
        return rounded_box_mass(b.half_extents, b.radius);
    }
    std::optional<PrimitiveMass> operator()(const MassShape::Cylinder& c) const {
        const float radius = c.radius + c.padding;
        const float half_length = c.half_length + c.padding;
        const float radius_squared = radius * radius;
        const float transverse =
            rs::mul_add(half_length * half_length, 4.0f, radius_squared * 3.0f) * ONE_TWELFTH;
        return PrimitiveMass{
            Vector3{transverse, transverse, radius_squared * 0.5f},
            ((half_length * radius) * radius) * TWO_PI,
        };
    }
};
}  // namespace

std::optional<PrimitiveMass> primitive_mass(MassShape shape) {
    return std::visit(Dispatch{}, shape.value);
}

}  // namespace s3::physics::mass::primitive_mass_mod
