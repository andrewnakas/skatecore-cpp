// Port of skate-core/src/physics/board_probes.rs.
#include "s3/core/physics/board_probes.hpp"

namespace s3::physics::board_probes {

using board_motion_output::dot;
using board_motion_output::inverse_length_squared;

namespace {
Vector3 cross(Vector3 a, Vector3 b) {
    return Vector3{
        rs::mul_add(-a.z, b.y, a.y * b.z),
        rs::mul_add(-a.x, b.z, a.z * b.x),
        rs::mul_add(-a.y, b.x, a.x * b.y),
    };
}
}  // namespace

std::optional<WheelLine> wall_probe(WallLineInput input) {
    if (!(input.state == 100 || input.state == 101) || !(rs::abs(input.contact_normal.y) < 0.5f) ||
        !(dot(input.skater_up, input.contact_normal) > rs::from_bits(0x3f35c28fu)) ||
        !(input.time > rs::from_bits(0x3c23d70au))) {
        return std::nullopt;
    }
    const Vector3 normal = input.contact_normal;
    const Vector3 start{
        rs::mul_add(normal.x, 0.05f, input.deck_position.x),
        rs::mul_add(normal.y, 0.05f, input.deck_position.y),
        rs::mul_add(normal.z, 0.05f, input.deck_position.z),
    };
    const Vector3 first = cross(Vector3{0.0f, 1.0f, 0.0f}, normal);
    Vector3 down = cross(first, normal);
    const float squared = dot(down, down);
    const float inverse = inverse_length_squared(squared, 2);
    const float magnitude = squared == 0.0f ? 0.0f : squared * inverse;
    down = magnitude > rs::from_bits(0x358637bdu) ? Vector3{down.x * inverse, down.y * inverse, down.z * inverse}
                                                  : Vector3::ZERO;
    return WheelLine{
        start,
        Vector3{rs::mul_add(down.x, 4.0f, start.x), rs::mul_add(down.y, 4.0f, start.y),
                rs::mul_add(down.z, 4.0f, start.z)},
    };
}

}  // namespace s3::physics::board_probes
