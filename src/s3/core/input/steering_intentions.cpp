// Port of skate-core/src/input/steering_intentions.rs.
// Complete local Turn/HardTurn/HardTurnCrouch emission dataflow in 825999F0.
#include "s3/core/input/steering_intentions.hpp"

#include "s3/core/input/angle.hpp"
#include "s3/core/input/controller.hpp"

namespace s3::input::steering_intentions {

using rs::from_bits;

SteeringIntentions produce(std::array<float, 2> left, std::uint32_t actor_flags_1908) {
    const float x = left[0];
    const float y = left[1];
    const float angle = angle::left_stick_angle(x, y);
    const float absolute = rs::abs(angle);
    const float length = (x == 0.0f && y == 0.0f) ? 0.0f : controller::magnitude(rs::mul_add(y, y, x * x));
    const float signed_length = x >= 0.0f ? length : -length;
    float turn;
    if (y > 0.0f) {
        if (absolute > from_bits(0x40433333u)) {
            turn = 0.0f;
        } else if (absolute > from_bits(0x40166666u)) {
            turn = ((from_bits(0x40433333u) - absolute) * from_bits(0x3fb6db6du)) * signed_length;
        } else {
            turn = signed_length;
        }
    } else {
        turn = x;
    }
    const bool active = length > from_bits(0x3f666666u) && !(absolute > from_bits(0x3fb1eb85u)) &&
                        !(absolute < from_bits(0x3f68f5c3u));
    float hard = (absolute - from_bits(0x3fb1eb85u)) * from_bits(0xc0055556u);
    const float minimum = from_bits(0x3a83126fu);
    hard = hard - minimum >= 0.0f ? hard : minimum;
    hard = angle >= 0.0f ? hard : -hard;
    const bool allowed = (actor_flags_1908 & 1u) == 0;
    SteeringIntentions out;
    if (turn != 0.0f && allowed) out.turn = turn;
    if (active && allowed) out.hard_turn = hard;
    if (active && allowed) out.hard_turn_crouch = rs::abs(hard * from_bits(0x3f59999au));
    return out;
}

}  // namespace s3::input::steering_intentions
