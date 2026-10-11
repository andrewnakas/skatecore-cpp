// Port of skate-game/src/physics/grind_chromosome/pose.rs.
#include "s3/game/physics/grind_chromosome/pose.hpp"

#include "s3/core/math.hpp"
#include "s3/core/physics/board_motion_output.hpp"

namespace s3::game::physics::grind_chromosome::pose {

///82DEED98: twist uses Basic128, not Basic32 or a substituted deck-forward.
bool straight(const Input& input) {
    const float d = dot(input.basic_twist_axis_128, input.normal);
    const V scaled = {input.normal[0] * d, input.normal[1] * d, input.normal[2] * d, input.normal[3] * d};
    const V projected = sub(input.basic_twist_axis_128, scaled);
    const float length = ::s3::physics::board_motion_output::length(
        ::s3::math::Vector3::new_(projected[0], projected[1], projected[2]));
    if (length <= 0.01f) {
        return true;
    }
    float inverse = rs::recip(length);
    for (int k = 0; k < 2; ++k) {
        const float correction = rs::mul_add(-inverse, length, 1.0f);
        inverse = rs::mul_add(inverse, correction, inverse);
    }
    const V unit = {projected[0] * inverse, projected[1] * inverse, projected[2] * inverse, projected[3] * inverse};
    const float along = dot(unit, input.direction);
    return rs::abs(along) > 0.9397f;
}

///82DEEEE0: sign the Basic32 axis from Basic48 minus contact.
bool low(const Input& input) {
    const V axis = signed_(input.basic_forward_32,
                           dot(sub(input.basic_position_48, input.point), input.basic_forward_32) > 0.0f);
    return dot(input.normal, axis) <= -0.1f;
}

}  // namespace s3::game::physics::grind_chromosome::pose
