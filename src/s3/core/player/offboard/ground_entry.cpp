// Port of skate-core/src/player/offboard/ground_entry.rs.
#include "s3/core/player/offboard/ground_entry.hpp"

#include "s3/core/player/offboard/controller/output.hpp"

namespace s3::player::offboard::ground_entry {

using namespace math;

Placement State::enter(const Input& input) {
    *this = State{};
    flags_144_to_150[4] = input.previous_state_2504 == 502;
    const Vector up = input.animation_frame[1];
    const Vector velocity = input.processed_velocity_608;
    const Vector planar = sub(velocity, scale(up, dot(up, velocity)));
    Frame frame = input.animation_frame;
    if ((input.processed_flags_2484 & 0x4000u) != 0) {
        flags_144_to_150[6] = true;
        duration_180 = input.requested_duration_2896;
        float angle = input.requested_angle_2936;
        if ((input.processed_flags_2476 & 4u) != 0) {
            angle *= -1.0f;
        }
        const Vector direction =
            length(planar) < rs::from_bits(0x3dcccccdu) ? input.animation_frame[2] : planar;
        const Frame basis = controller::build_frame(up, direction);
        frame[0] = basis[0];
        frame[1] = basis[1];
        frame[2] = basis[2];
        const Vector rotated = rotate(input.animation_frame[2], up, angle);
        angle_172 = wrap(signed_angle(direction, rotated, up));
        float original = wrap(signed_angle(input.animation_frame[2], direction, up));
        if (!(rs::abs(angle) <= rs::from_bits(0x3fc90fdbu)) && !(original * angle >= 0.0f)) {
            original += original > 0.0f ? -TAU : TAU;
        }
        angular_velocity_176 = (angle_172 + original) / duration_180;
    }
    frame_80 = frame;
    // Source sets149 after board-manager entry and before skeleton mode4.
    flags_144_to_150[5] = true;
    if (input.previous_state_2504 == 501) {
        flags_144_to_150[1] = rs::from_bits(0x3f35c28fu) > input.previous_frame_up_208[1];
    }
    Placement out;
    out.frame = frame;
    out.planar_velocity = planar;
    out.body_position = input.body_position_15872;
    return out;
}

}  // namespace s3::player::offboard::ground_entry
