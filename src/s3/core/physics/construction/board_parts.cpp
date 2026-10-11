// Port of skate-core/src/physics/construction/board_parts.rs.
#include "s3/core/physics/construction/board_parts.hpp"

namespace s3::physics::mass::board_parts {

PartMassInput wheel_mass_input(WheelMassSettings settings) {
    return PartMassInput{
        MassShape::Sphere{settings.radius},
        settings.mass * settings.mass_factor,
    };
}

PartMassInput truck_mass_input(TruckMassSettings settings) {
    return PartMassInput{
        MassShape::Capsule{
            (settings.wheel_radius * settings.radius_scalar) * 0.5f,
            (settings.wheel_x_distance * settings.half_height_scalar) * 0.5f,
        },
        settings.mass * settings.mass_factor,
    };
}

std::optional<ForwardMassProperties> forward_mass_properties(PartMassInput input) {
    const auto primitive = primitive_mass_mod::primitive_mass(input.shape);
    if (!primitive) {
        return std::nullopt;
    }
    const float mass =
        input.requested_mass < rs::f32::MIN_POSITIVE ? primitive->volume : input.requested_mass;
    const Vector3 moments = primitive->moments_per_unit_mass;
    return ForwardMassProperties{
        *primitive,
        mass,
        Vector3{moments.x * mass, moments.y * mass, moments.z * mass},
    };
}

}  // namespace s3::physics::mass::board_parts
