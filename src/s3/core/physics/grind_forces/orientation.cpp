// Port of skate-core/src/physics/grind_forces/orientation.rs.
#include "s3/core/physics/grind_forces/orientation.hpp"

#include "s3/core/animation/foot_ik.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::physics::grind_forces::orientation {

using board_motion_output::inverse_length_squared;

namespace {

V cross(const V& a, const V& b) {
    return V{
        rs::mul_add(-a[2], b[1], a[1] * b[2]),
        rs::mul_add(-a[0], b[2], a[2] * b[0]),
        rs::mul_add(-a[1], b[0], a[0] * b[1]),
        rs::mul_add(-a[3], b[3], a[3] * b[3]),
    };
}

V signed_(const V& v, bool positive) { return positive ? v : scale(v, -1.0f); }

V rotate(const V& axis, const V& value, float angle) {
    const auto [sine, cosine] = trigonometry::sin_cos(angle * 0.5f);
    const V q = scale(axis, sine);
    const V first = cross(q, value);
    V inner{};
    for (std::size_t i = 0; i < 4; ++i) inner[i] = rs::mul_add(cosine, value[i], first[i]);
    const V second = cross(q, inner);
    V out{};
    for (std::size_t i = 0; i < 4; ++i) out[i] = rs::mul_add(2.0f, second[i], value[i]);
    return out;
}

/// 82D40290: zero angles skip rotation; near-parallel support returns the entire
/// input frame. Neither branch introduces a replacement normal or heading.
Frame truck(const Input& i) {
    const float yaw = i.yaw_1504 * -0.68f;
    V forward = i.board[2];
    if (rs::abs(yaw) > 0.0f) {
        forward = rotate(i.normal, forward, yaw);
    }
    V right = cross(i.normal, forward);
    const float magnitude = length(right);
    if (magnitude <= 0.001f) {
        return i.board;
    }
    right = scale(right, reciprocal(magnitude));
    V up = cross(forward, right);
    const float pitch = (i.switched ? -i.pitch_1508 : i.pitch_1508) * 0.68f;
    if (rs::abs(pitch) > 0.0f) {
        forward = rotate(right, forward, pitch);
        up = cross(forward, right);
    }
    return Frame{right, up, forward, i.board[3]};
}

Frame orthonormal(const Frame& input) {
    Frame output{};
    for (std::size_t axis = 3; axis-- > 0;) {
        V value = input[axis];
        for (std::size_t previous = 3; previous-- > axis + 1;) {
            value = sub(value, scale(output[previous], dot3(output[previous], input[axis])));
        }
        output[axis] = scale(value, inverse_length_squared(dot3(value, value), 2));
    }
    return output;
}

}  // namespace

rs::Result<Target, const char*> target(std::uint32_t family, Input i) {
    Frame frame;
    if (family == 0 && rs::abs(i.pitch_1508) <= 0.12f) {
        const V forward = signed_(i.direction, dot3(i.direction, i.board[2]) > 0.0f);
        frame = Frame{cross(i.normal, forward), i.normal, forward, i.board[3]};
    } else if (family == 0 || family == 3) {
        frame = truck(i);
    } else if (family == 1 || family == 5) {
        const V right = signed_(i.direction, dot3(i.direction, i.board[0]) > 0.0f);
        const V up = family == 5 ? scale(i.normal, -1.0f) : i.normal;
        frame = Frame{right, up, cross(right, up), i.board[3]};
    } else if (family == 2 || family == 4) {
        const V across = cross(i.direction, i.normal);
        const V axis = signed_(i.direction, !(dot3(across, sub(i.board[3], i.point)) > 0.0f));
        // Original822F8618/822F8614,20/42degrees expressed in radians.
        const float angle = rs::from_bits(family == 4 ? 0x3f3ba866u : 0x3eb2b8c3u);
        const V up = rotate(axis, i.normal, angle);
        const V right = signed_(i.direction, dot3(i.direction, i.board[0]) > 0.0f);
        frame = Frame{right, up, cross(right, up), i.board[3]};
    } else {
        return rs::make_err("Unknown physical grind family");
    }
    return Target{frame, family == 0 ? 0.0f : 0.09f};
}

Frame blend(Frame current, Frame target_) {
    return animation::foot_ik::interpolate_native(orthonormal(current), orthonormal(target_), 0.1f).first;
}

}  // namespace s3::physics::grind_forces::orientation
