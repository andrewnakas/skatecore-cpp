// Port of skate-core/src/animation/foot_ik/math.rs.
#include "s3/core/animation/foot_ik/math.hpp"

#include "s3/core/input/angle.hpp"
#include "s3/core/physics/board_motion_output.hpp"
#include "s3/core/physics/native_arithmetic.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::animation::foot_ik::math {

using input::angle::atan;
using physics::board_motion_output::inverse_length_squared;
using physics::native_arithmetic::dot3;
using physics::native_arithmetic::reciprocal_estimate;
using physics::skeleton_animation_record::IDENTITY;
using rs::from_bits;
using rs::mul_add;

float reciprocal(float value, std::size_t refinements) {
    float inverse = reciprocal_estimate(value);
    for (std::size_t i = 0; i < refinements; ++i) {
        const float error = mul_add(-inverse, value, 1.0f);
        inverse = mul_add(inverse, error, inverse);
    }
    return inverse;
}

float length(Vector value) {
    const float squared = dot3(value, value);
    const float result = squared * inverse_length_squared(squared, 2);
    return squared == 0.0f ? 0.0f : result;
}

Vector normalize(Vector value) {
    const float inverse = inverse_length_squared(dot3(value, value), 2);
    return Vector{value[0] * inverse, value[1] * inverse, value[2] * inverse, value[3] * inverse};
}

Vector cross(Vector left, Vector right) {
    return Vector{
        mul_add(-left[2], right[1], left[1] * right[2]),
        mul_add(-left[0], right[2], left[2] * right[0]),
        mul_add(-left[1], right[0], left[0] * right[1]),
        mul_add(-left[3], right[3], left[3] * right[3]),
    };
}

Vector limit_length(Vector value, float limit) {
    const float magnitude = length(value);
    if (!(magnitude >= from_bits(0x37800000u))) {
        return value;
    }
    const float capped = limit - magnitude >= 0.0f ? magnitude : limit;
    const float inverse = reciprocal(magnitude, 2);
    return Vector{(value[0] * capped) * inverse, (value[1] * capped) * inverse,
                  (value[2] * capped) * inverse, (value[3] * capped) * inverse};
}

namespace {
/// 827B7400 selects the greatest diagonal. Its selected component is squared
/// before normalization (e.g. vmulfp at827B7488), unlike a common textbook
/// matrix-to-axis conversion. Preserve the native formula and tie ordering.
Vector half_turn_axis(const Transform& rotation) {
    std::size_t selected;
    if (rotation[0][0] > rotation[1][1]) {
        selected = rotation[0][0] > rotation[2][2] ? 0 : 2;
    } else if (rotation[1][1] > rotation[2][2]) {
        selected = 1;
    } else {
        selected = 2;
    }
    Vector axis{0.0f, 0.0f, 0.0f, 0.0f};
    for (std::size_t lane = 0; lane < 3; ++lane) {
        if (lane == selected) {
            const float diagonal = 1.0f + rotation[selected][selected];
            axis[lane] = diagonal * diagonal;
        } else {
            axis[lane] = rotation[selected][lane] + rotation[lane][selected];
        }
    }
    axis[3] = axis[0];
    return normalize(axis);
}
}  // namespace

std::pair<Vector, float> rotation_axis_angle(const Transform& rotation) {
    const Vector skew{
        rotation[1][2] - rotation[2][1],
        rotation[2][0] - rotation[0][2],
        rotation[0][1] - rotation[1][0],
        0.0f,
    };
    const float sine_twice = length(skew);
    const float cosine_twice = ((rotation[0][0] + rotation[1][1]) + rotation[2][2]) - 1.0f;
    Vector axis{0.0f, 0.0f, 0.0f, 0.0f};
    if (sine_twice > 0.0f) {
        const float inverse = reciprocal(sine_twice, 2) * 1.0f;
        axis = Vector{skew[0] * inverse, skew[1] * inverse, skew[2] * inverse, skew[3] * inverse};
    }
    const float ratio = mul_add(sine_twice, reciprocal(cosine_twice, 1), 0.0f);
    float angle = atan(ratio);
    if (cosine_twice < 0.0f) {
        angle += rs::copysign(from_bits(0x40490FDBu), sine_twice);
    }
    if (cosine_twice == 0.0f) {
        angle = rs::copysign(from_bits(0x3FC90FDBu), sine_twice);
    }
    // Literal word 820CFC28, loaded with lvlx. Do not replace this with a
    // conventional numerical tolerance.
    if (sine_twice <= from_bits(0x00200000u) && cosine_twice <= 0.0f) {
        axis = half_turn_axis(rotation);
    }
    return {axis, angle};
}

Transform axis_rotation(Vector axis, float angle) {
    const auto [sine, cosine] = trigonometry::sin_cos(angle);
    const float complement = 1.0f - cosine;
    const float x = axis[0];
    const float y = axis[1];
    const float z = axis[2];
    const float sx = sine * x;
    const float sy = sine * y;
    const float sz = sine * z;
    const float tx = complement * x;
    const float ty = complement * y;
    const float tz = complement * z;
    const float xx = mul_add(tx, x, cosine);
    const float yy = mul_add(ty, y, cosine);
    const float zz = mul_add(tz, z, cosine);
    const float xy_positive = mul_add(tx, y, sz);
    const float xy_negative = ty * x - sz;
    const float xz_positive = mul_add(tz, x, sy);
    const float xz_negative = tx * z - sy;
    const float yz_positive = mul_add(ty, z, sx);
    const float yz_negative = tz * y - sx;
    return Transform{{
        {xx, xy_positive, xz_negative, xx},
        {xy_negative, yy, yz_positive, xy_negative},
        {xz_positive, yz_negative, zz, xz_positive},
        {0.0f, 0.0f, 0.0f, 0.0f},
    }};
}

std::pair<Transform, float> interpolate(const Transform& a, const Transform& b, float weight) {
    if (weight >= 1.0f) {
        return {b, 0.0f};
    }
    Transform relative = IDENTITY;
    for (std::size_t column = 0; column < 3; ++column) {
        for (std::size_t lane = 0; lane < 4; ++lane) {
            const float x = a[0][column] * b[0][lane];
            const float y = mul_add(a[1][column], b[1][lane], x);
            relative[column][lane] = mul_add(a[2][column], b[2][lane], y);
        }
    }
    const auto [axis, angle] = rotation_axis_angle(relative);
    if (weight <= 0.0f) {
        return {a, angle};
    }
    Transform output = IDENTITY;
    if (angle < from_bits(0x3D0EFA35u)) {
        for (std::size_t column = 0; column < 3; ++column) {
            Vector lerped{};
            for (std::size_t lane = 0; lane < 4; ++lane) {
                lerped[lane] = mul_add(b[column][lane] - a[column][lane], weight, a[column][lane]);
            }
            output[column] = normalize(lerped);
        }
    } else {
        const Transform rotation = axis_rotation(axis, angle * weight);
        for (std::size_t column = 0; column < 3; ++column) {
            for (std::size_t lane = 0; lane < 4; ++lane) {
                const float x = a[column][0] * rotation[0][lane];
                const float y = mul_add(a[column][1], rotation[1][lane], x);
                output[column][lane] = mul_add(a[column][2], rotation[2][lane], y);
            }
        }
    }
    for (std::size_t lane = 0; lane < 4; ++lane) {
        output[3][lane] = mul_add(b[3][lane] - a[3][lane], weight, a[3][lane]);
    }
    return {output, angle - angle * weight};
}

Transform interpolate_affine(const Transform& a, const Transform& b, float weight) {
    Vector translation{};
    for (std::size_t lane = 0; lane < 4; ++lane) {
        translation[lane] = mul_add(a[3][lane], 1.0f - weight, b[3][lane] * weight);
    }
    Transform output = interpolate(a, b, weight).first;
    output[3] = translation;
    return output;
}

Transform inverse_affine(const Transform& frame) {
    const std::array<Vector, 3> cofactors = {
        cross(frame[1], frame[2]),
        cross(frame[2], frame[0]),
        cross(frame[0], frame[1]),
    };
    const float inverse = reciprocal(dot3(frame[0], cofactors[0]), 2);
    Transform result{};
    for (std::size_t column = 0; column < 4; ++column) {
        if (column == 3) {
            result[column] = Vector{0.0f, 0.0f, 0.0f, 0.0f};
        } else {
            result[column] = Vector{
                cofactors[0][column] * inverse,
                cofactors[1][column] * inverse,
                cofactors[2][column] * inverse,
                cofactors[1][column] * inverse,
            };
        }
    }
    Vector negative{};
    for (std::size_t i = 0; i < 4; ++i) {
        negative[i] = from_bits(rs::to_bits(frame[3][i]) ^ 0x80000000u);
    }
    for (std::size_t lane = 0; lane < 4; ++lane) {
        const float x = negative[0] * result[0][lane];
        const float y = mul_add(negative[1], result[1][lane], x);
        result[3][lane] = mul_add(negative[2], result[2][lane], y);
    }
    return result;
}

}  // namespace s3::animation::foot_ik::math
