// Port of skate-core/src/player/wipeout_state/orientation.rs (whole file).
#include "s3/core/player/wipeout_state/orientation.hpp"

#include "s3/core/physics/board_motion_output.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::player::wipeout_state::orientation {

using trigonometry::sin_cos;

V alignment_up(float x_degrees, float y_degrees) {
    const float radians = rs::from_bits(0x3C8EFA35u);
    const auto [sx, cx] = sin_cos((x_degrees * radians) * 0.5f);
    const auto [sy, cy] = sin_cos((y_degrees * radians) * 0.5f);
    const auto [sz, cz] = sin_cos(0.0f);
    const float cxcz = cx * cz;
    const float sxcz = sx * cz;
    const float sxsz = sx * sz;
    const float cxsz = cx * sz;
    // 82D3C8B0 word114D4AAE: D10=A13*C10+B9, not A13*B9+C10.
    const float qy = rs::mul_add(cy, sxsz, sy * cxcz);
    // 82D3C8B4 word11AD42EE: D13=A13*C11+B8.
    const float qw = rs::mul_add(cy, cxcz, sy * sxsz);
    const float qz = cy * cxsz - sy * sxcz;
    const float qx = cy * sxcz - sy * cxsz;
    return rotate(V{qx, qy, qz, qw}, V{0.0f, 1.0f, 0.0f, 0.0f});
}

V rotate(V quaternion, V vector) {
    const V first = math::cross(quaternion, vector);
    const V middle = math::madd(vector, quaternion[3], first);
    return math::madd(math::cross(quaternion, middle), 2.0f, vector);
}

float signed_angle(V left, V right, V axis) {
    using physics::board_motion_output::inverse_length_squared;
    const float a = math::dot(left, left);
    const float b = math::dot(right, right);
    if (!(a > rs::from_bits(0x38D1B717u) && b > rs::from_bits(0x38D1B717u))) {
        return 0.0f;
    }
    left = math::scale(left, inverse_length_squared(a, 1));
    right = math::scale(right, inverse_length_squared(b, 1));
    const float angle = trigonometry::acos(math::clamp(math::dot(left, right), -1.0f, 1.0f));
    if (math::dot(math::cross(left, right), axis) < 0.0f) {
        return rs::from_bits(0x40C90FDBu) - angle;
    }
    return angle;
}

float projected_angle(V left, V right, V axis) {
    if (!(math::dot(left, left) * math::dot(right, right) > rs::from_bits(0x37800000u))) {
        return 0.0f;
    }
    return signed_angle(math::sub(left, math::scale(axis, math::dot(axis, left))),
                        math::sub(right, math::scale(axis, math::dot(axis, right))), axis);
}

}  // namespace s3::player::wipeout_state::orientation
