// Port of skate-core/src/physics/skeleton_output/board.rs.
#include "s3/core/physics/skeleton_output/board.hpp"

#include "s3/core/math.hpp"
#include "s3/core/physics/board_motion_output.hpp"
#include "s3/core/physics/native_arithmetic.hpp"
#include "s3/core/riding/collision_response.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::physics::skeleton_output::board {

using board_motion_output::inverse_length_squared;
using native_arithmetic::dot3;
using skeleton_animation_record::compose_affine;
using V4 = std::array<float, 4>;

namespace {

float angle(const V4& a, const V4& b, const V4& axis) {
    auto vector = [](const V4& v) { return math::Vector3{v[0], v[1], v[2]}; };
    return riding::collision_response::signed_angle(vector(a), vector(b), vector(axis));
}
V4 inverse_direction(const Transform& frame, const V4& value) {
    V4 output{0.0f, 0.0f, 0.0f, 0.0f};
    for (std::size_t lane = 0; lane < 3; ++lane) {
        const float x = value[0] * frame[lane][0];
        const float y = rs::mul_add(value[1], frame[lane][1], x);
        output[lane] = rs::mul_add(value[2], frame[lane][2], y);
    }
    return output;
}
V4 normalize_safe(const V4& value) {
    const float squared = dot3(value, value);
    const float inverse = inverse_length_squared(squared, 2);
    const float length = squared == 0.0f ? 0.0f : squared * inverse;
    if (length > rs::from_bits(0x358637BDu)) {
        return V4{value[0] * inverse, value[1] * inverse, value[2] * inverse, value[3] * inverse};
    }
    return V4{0.0f, 0.0f, 0.0f, 0.0f};
}
V4 subtract(const V4& a, const V4& b) { return V4{a[0] - b[0], a[1] - b[1], a[2] - b[2], a[3] - b[3]}; }
float wrap(float angle) {
    const float turns = angle * rs::from_bits(0x3E22F983u);
    const float fraction = turns - rs::floor(turns);
    return (fraction - (fraction > 0.5f ? 1.0f : 0.0f)) * rs::from_bits(0x40C90FDBu);
}
float clamp(float value, float minimum, float maximum) {
    const float low = minimum - value >= 0.0f ? minimum : value;
    return maximum - low >= 0.0f ? low : maximum;
}
Transform rotation_x(float angle) {
    const auto [sine, cosine] = trigonometry::sin_cos(angle);
    // vperm822FB890 replicates X in W; vrlimi2 replaces only Z.
    return Transform{{
        {1.0f, 0.0f, 0.0f, 1.0f},
        {0.0f, cosine, sine, 0.0f},
        {0.0f, -sine, cosine, 0.0f},
        {0.0f, 0.0f, 0.0f, 0.0f},
    }};
}

}  // namespace

void publish(const Input& input, const Settings& settings, BoneIndices bones, Slice<Transform> locals) {
    const V4 y{0.0f, 1.0f, 0.0f, 0.0f};
    const V4 z{0.0f, 0.0f, 1.0f, 0.0f};
    std::array<Transform, 4> wheels{};
    for (std::size_t wheel = 0; wheel < 4; ++wheel) {
        const V4 up = inverse_direction(input.bodies[4 + wheel / 2], input.bodies[wheel][1]);
        wheels[wheel] = rotation_x(angle(up, y, z));
    }
    // 82DB9C60/9E00 choose opposite axle directions at the two ends.
    V4 front = inverse_direction(input.skeleton_board,
                                 normalize_safe(subtract(input.bodies[0][3], input.bodies[1][3])));
    V4 back = inverse_direction(input.skeleton_board,
                                normalize_safe(subtract(input.bodies[3][3], input.bodies[2][3])));
    if (0.5f > dot3(front, front)) {
        front = input.truck_frames[0][2];
    }
    if (0.5f > dot3(back, back)) {
        const V4& t = input.truck_frames[1][2];
        back = V4{-t[0], -t[1], -t[2], -t[3]};
    }
    const V4 negative_x{-1.0f, -0.0f, -0.0f, -0.0f};
    const float back_angle = wrap(angle(back, negative_x, input.truck_frames[1][0]));
    const float front_angle = wrap(angle(front, negative_x, input.truck_frames[0][0]));
    const float wobble = input.deck_wobble_tilt * settings.truck_tilt_wobble_scalar;
    const float back_tilt = clamp(rs::mul_add(-settings.truck_tilt_scalar, back_angle, -wobble),
                                  -settings.truck_tilt_max_angle, settings.truck_tilt_max_angle);
    const float front_tilt = clamp(rs::mul_add(-settings.truck_tilt_scalar, front_angle, wobble),
                                   -settings.truck_tilt_max_angle, settings.truck_tilt_max_angle);
    const std::array<std::tuple<std::size_t, std::size_t, float>, 2> trucks = {{
        {0, bones.front_truck, front_tilt},
        {1, bones.back_truck, back_tilt},
    }};
    for (const auto& [side, bone, tilt] : trucks) {
        const float compression = input.average_compression[side] < rs::from_bits(0x3A83126Fu)
                                      ? 0.0f
                                      : input.average_compression[side];
        const float displacement =
            clamp(compression - input.deck_wobble_squish, 0.0f, settings.truck_displacement_max);
        const Transform rotated = compose_affine(locals[bone], rotation_x(tilt));
        // 82DBA130..A318 left-multiplies the rotated local by translation.
        const Transform translation{{
            {1.0f, 0.0f, 0.0f, 0.0f},
            {0.0f, 1.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, 1.0f, 0.0f},
            {0.0f, displacement, 0.0f, 0.0f},
        }};
        locals[bone] = compose_affine(translation, rotated);
    }
    // Native physical-body order and animation-name order differ.
    const std::array<std::size_t, 4> wheel_bones = {bones.front_left_wheel, bones.front_right_wheel,
                                                    bones.back_left_wheel, bones.back_right_wheel};
    for (std::size_t wheel = 0; wheel < 4; ++wheel) {
        const std::size_t bone = wheel_bones[wheel];
        locals[bone] = compose_affine(locals[bone], wheels[wheel]);
    }
}

}  // namespace s3::physics::skeleton_output::board
