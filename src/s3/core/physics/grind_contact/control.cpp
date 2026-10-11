// Port of skate-core/src/physics/grind_contact/control.rs.
#include "s3/core/physics/grind_contact/control.hpp"

#include <tuple>

#include "s3/core/trigonometry.hpp"

namespace s3::physics::grind_contact::control {

namespace {
///82D862B8: signed .25 dead zone, remaining range scaled by4/3.
float deadzone(float v) {
    if (v > 0.25f) {
        return (v - 0.25f) * (4.0f / 3.0f);
    } else if (v < -0.25f) {
        return (v + 0.25f) * (4.0f / 3.0f);
    }
    return 0.0f;
}
}  // namespace

void Control::update(std::uint32_t kind, V board_forward, V normal, bool front, bool switched, bool hanging_back,
                     float translation, float nudge, float up_down, float grab_min_height) {
    float yaw_in, pitch_in;
    bool modified;
    if (kind == 0) {
        yaw_in = 0.0f;
        pitch_in = up_down * up_down * up_down;
        modified = false;
    } else if (kind == 3) {
        const bool selected = front != switched;
        const float tilt = dot3(normal, board_forward);
        const float height = (selected ? -tilt : tilt) * (switched ? -1.0f : 1.0f);
        float input = up_down;
        if (height > 0.0f && rs::abs(up_down) <= 0.01f && height > -0.25f) {
            input = selected ? -0.71f : 0.71f;
        }
        float target = input;
        if (rs::abs(tilt) > 0.26f) {
            target = selected ? rs::min(input, 0.0f) : rs::max(input, 0.0f);
        }
        if (height < -0.25f) {
            target = selected ? rs::max(input, 0.0f) : rs::min(input, 0.0f);
        }
        const float sign = (front ? 1.0f : -1.0f) * (switched ? -1.0f : 1.0f);
        if (grab_min_height > 0.0f && height <= grab_min_height) {
            const float value = (grab_min_height - height) * sign * 10.0f;
            target = sign > 0.0f ? rs::min(value, sign) : rs::max(value, sign);
        }
        if (hanging_back) {
            target = rs::clamp(target + sign * 0.5f, -1.0f, 1.0f);
        }
        //82D8A238/240: round the nudge product, then fused translation.
        const float y = rs::clamp(rs::mul_add(deadzone(translation), 0.85f, deadzone(nudge) * 0.79f), -0.85f, 0.85f);
        yaw_in = y * y * y;
        pitch_in = target * target * target;
        modified = target != input;
    } else {
        yaw_in = 0.0f;
        pitch_in = 0.0f;
        modified = false;
    }
    const float response = modified ? 0.76f : 0.91f;
    yaw = rs::mul_add(yaw, 0.91f, yaw_in * rs::from_bits(0x3db851e8u));
    pitch = rs::mul_add(1.0f - response, pitch_in, pitch * response);
    if (rs::abs(yaw) < 0.0001f) {
        yaw = 0.0f;
    }
    if (rs::abs(pitch) < 0.0001f) {
        pitch = 0.0f;
    }
}

V rotate(V axis, V value, float angle) {
    const auto [s, c] = trigonometry::sin_cos(angle * 0.5f);
    const V q = scale(axis, s);
    return add(value, scale(cross(q, add(scale(value, c), cross(q, value))), 2.0f));
}

std::array<V, 4> truck_frame(std::array<V, 4> board, V normal, const Control& control, bool switched) {
    V forward = rotate(normal, board[2], control.yaw * -0.68f);
    V right = cross(normal, forward);
    const float length = rs::sqrt(dot3(right, right));
    if (length <= 0.001f) {
        return board;
    }
    right = scale(right, rs::recip(length));
    forward = rotate(right, forward, control.pitch * (switched ? -0.68f : 0.68f));
    return {right, cross(forward, right), forward, board[3]};
}

std::array<V, 4> tip_frame(std::array<V, 4> board, V direction, V normal, V point, bool backslash) {
    const V across = cross(direction, normal);
    const V axis = scale(direction, dot3(across, sub(board[3], point)) > 0.0f ? -1.0f : 1.0f);
    const float angle = backslash ? rs::from_bits(0x3f3ba866u) : rs::from_bits(0x3eb2b8c3u);
    const V up = rotate(axis, normal, angle);
    const V right = scale(direction, dot3(direction, board[0]) > 0.0f ? 1.0f : -1.0f);
    return {right, up, cross(right, up), board[3]};
}

}  // namespace s3::physics::grind_contact::control
