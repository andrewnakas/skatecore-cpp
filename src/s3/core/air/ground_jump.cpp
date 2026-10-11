// Port of skate-core/src/air/ground_jump/mod.rs.
#include "s3/core/air/ground_jump.hpp"

#include "s3/core/air/ground_jump/math.hpp"
#include "s3/core/air/state/math.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::air::ground_jump {

using namespace ground_jump::math;
using state::math::AirMath;
using trigonometry::acos;

GroundJump calculate(GroundJumpInput input, GroundJumpMode mode, const GroundJumpSettings& s) {
    const bool hippy = (input.flags_2480 & 0x00001000u) != 0;
    if ((input.flags_2468 & 0x00400000u) == 0 && !hippy) {
        return GroundJump{};
    }
    const Vector normal = input.filtered_ground_normal;
    const float y_scalar = s.y_scalar_vs_normal_y.evaluate(normal[1]);
    const Vector current = planar(input.current_velocity, normal);
    Vector prepared = planar(input.prepared_velocity, normal);
    if (dot(sub(current, prepared), sub(current, prepared)) > 7.0f) {
        const float agreement = vector_clamp(dot(normalize(current), normalize(prepared)), 0.0f, 1.0f);
        const float fraction = acos(agreement) * reciprocal(rs::from_bits(0x40490fdbu));
        const float bound = length(prepared) * s.speed_scalar_vs_angle.evaluate(fraction);
        prepared = AirMath{}.clamp_vector_within_max_length(current, bound);
    }
    Vector velocity;
    if ((input.flags_2488 & 0x10000000u) != 0) {
        velocity = madd(input.effective_forward, 2.3f, prepared);
        velocity[1] += 1.0f;
    } else {
        const float minimum = hippy                                  ? s.hippy_minimum_height
                              : (input.flags_2484 & 0x800u) != 0 ? mode.minimum_height_64
                                                                     : mode.minimum_height_68;
        const float speed_fraction = clamp(input.surface_speed / s.speed_response_max_speed, 0.0f, 1.0f);
        const float low = rs::mul_add(s.minimum_height_vs_speed.evaluate(speed_fraction),
                                      minimum - s.absolute_minimum_height, s.absolute_minimum_height);
        const float high = rs::mul_add(s.maximum_height_vs_speed.evaluate(speed_fraction),
                                       (hippy ? s.hippy_maximum_height : mode.maximum_height) -
                                           s.absolute_minimum_height,
                                       s.absolute_minimum_height);
        const float height = rs::mul_add(low, 1.0f - input.jump_strength, high * input.jump_strength);
        const float current_height =
            dot(sub(input.animation_com_position, input.ground_reference_position), input.reference_up);
        const float remaining = maximum(height - current_height, 0.0f);
        const float square = (remaining * input.gravity_y) * -2.0f;
        const float speed = square_root(square);
        const float angle = acos(vector_clamp(normal[1], -1.0f, 1.0f));
        const float fraction = clamp(angle * rs::from_bits(0x3f22f983u), 0.0f, 1.0f);
        const float response = maximum(s.vertical_response.evaluate(fraction), s.minimum_scalar);
        velocity = madd(normal, speed * response, prepared);
    }
    const float bonus = clamp((y_scalar - 1.0f) * velocity[1], 0.0f, s.maximum_y_bonus);
    velocity[1] += bonus;
    const float projected = dot(velocity, normal);
    const Vector side = normalize(cross(UP, input.forward));
    const float x = -((input.jump_controls[0] * s.adjust_x_factor) * projected);
    const float z = (input.jump_controls[1] * s.adjust_z_factor) * projected;
    const Vector correction = madd(side, x, scale(cross(side, UP), z));
    GroundJump out;
    out.velocity = add(velocity, correction);
    out.scalar_16 = 0.0f;
    out.active = true;
    return out;
}

}  // namespace s3::air::ground_jump
