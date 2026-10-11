// Port of skate-core/src/physics/board_input_output.rs.
#include "s3/core/physics/board_input_output.hpp"

#include "s3/core/riding/collision_response.hpp"

namespace s3::physics::board_input_output {

using board::BodyId;
using board_motion_output::dot;
using board_motion_output::scale;
using board_motion_output::subtract;

namespace {
Vector3 reject(Vector3 vector, Vector3 normal) { return subtract(vector, scale(normal, dot(vector, normal))); }
player::input_phase::RawVector bits(Vector3 vector) {
    return rs::to_bits(std::array<float, 4>{vector.x, vector.y, vector.z, 0.0f});
}
}  // namespace

void publish_board_input(PhysicalPlayerInput& out, const BoardMotionOutput& motion, const BoardGroundState& contacts,
                         BoardInputFrame frame) {
    const auto side = motion.effective_basis.columns[0];
    const float sign = (frame.processed_flags_2476 & 4u) != 0 ? -1.0f : 1.0f;
    out.board_reckoning_side_176 =
        rs::to_bits(std::array<float, 4>{side[0] * sign, side[1] * sign, side[2] * sign, 0.0f});
    out.skateboard.vector_64 = bits(motion.angular_velocity);
    out.skateboard.vector_80 = bits(motion.linear_velocity);
    out.skateboard.vector_96 = bits(motion.ground_velocity);
    out.skateboard.scalar_160 = motion.speed;
    out.skateboard.scalar_164 = motion.ground_speed;
    out.skateboard.scalar_168 = motion.forward_speed;
    out.skateboard.scalar_172 = travel_angle(frame.processed_forward, motion.ground_velocity,
                                             frame.reckoning_normal_1216, frame.reckoning_ground_up);
    // 82C02A80 never writes Motion128/144. Preserve the values from the
    // output reset and any other physical component, rather than inventing them.
    out.ground.vector_64 = bits(frame.retained_ground_normal_112);
    out.ground.vector_96 = bits(contacts.wheel_normal);
    for (std::size_t i = 0; i < 4; ++i) {
        out.collision.wheel_contact_3296_3299[i] = static_cast<std::uint8_t>(contacts.parts[i].in_contact);
    }
    out.collision.wheel_count_0 = static_cast<std::uint32_t>(contacts.wheel_contact_count);
    out.collision.flag_3472 = static_cast<std::uint8_t>(contacts.parts[board::index(BodyId::FrontTruck)].in_contact ||
                                                        contacts.parts[board::index(BodyId::BackTruck)].in_contact);
    out.collision.flag_3475 = static_cast<std::uint8_t>(contacts.parts[board::index(BodyId::Deck)].in_contact);
    out.collision.flag_3477 = static_cast<std::uint8_t>(
        out.collision.wheel_count_0 > 0 || out.collision.flag_3472 != 0 || out.collision.flag_3475 != 0);
}

float travel_angle(Vector3 forward, Vector3 velocity, Vector3 normal, Vector3 ground_up) {
    Vector3 projected_forward = reject(forward, normal);
    if (!(dot(projected_forward, velocity) > 0.0f)) {
        projected_forward = scale(projected_forward, -1.0f);
    }
    const float angle =
        dot(projected_forward, projected_forward) * dot(velocity, velocity) > rs::from_bits(0x37800000u)
            ? riding::collision_response::signed_angle(reject(projected_forward, ground_up),
                                                       reject(velocity, ground_up), ground_up)
            : 0.0f;
    const float turns = angle * rs::from_bits(0x3e22f983u);
    const float fraction = turns - rs::floor(turns);
    return (fraction - (fraction > 0.5f ? 1.0f : 0.0f)) * rs::from_bits(0x40c90fdbu);
}

}  // namespace s3::physics::board_input_output
