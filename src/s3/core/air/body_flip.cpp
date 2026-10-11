// Port of skate-core/src/air/body_flip.rs.
#include "s3/core/air/body_flip.hpp"

#include "s3/core/trigonometry.hpp"

namespace s3::air::body_flip {

namespace {

float clamp(float value, float lower, float upper) {
    value = lower - value >= 0.0f ? lower : value;
    return upper - value >= 0.0f ? value : upper;
}

Matrix rotation(std::array<float, 4> axis, float angle) {
    const float x = axis[0];
    const float y = axis[1];
    const float z = axis[2];
    const auto [s, c] = trigonometry::sin_cos(angle);
    const float t = 1.0f - c;
    const float tx = t * x, ty = t * y, tz = t * z;
    const float sx = s * x, sy = s * y, sz = s * z;
    const float xx = rs::mul_add(tx, x, c);
    const float yx = ty * x - sz;
    const float zx = rs::mul_add(tz, x, sy);
    // The native 822FB890 permute repeats column.x into lane w. It does not
    // create a renderer-style affine matrix with zero w basis lanes.
    return Matrix{{
        {xx, rs::mul_add(tx, y, sz), tx * z - sy, xx},
        {yx, rs::mul_add(ty, y, c), rs::mul_add(ty, z, sx), yx},
        {zx, tz * y - sx, rs::mul_add(tz, z, c), zx},
        {0.0f, 0.0f, 0.0f, 0.0f},
    }};
}

}  // namespace

void update(BodyFlipState& state, const BodyFlipSettings& settings, const BodyFlipInput& input) {
    const float fallback = settings.missing_attribute_value;
    const float smoothing = settings.smoothing.value_or(fallback);
    const float maximum = settings.maximum_speed.value_or(fallback);
    const float spin_scale = settings.spin_scale.value_or(fallback);
    state.requested_speed = input.requested_speed;
    float speed = rs::mul_add(input.requested_speed - state.speed, smoothing, state.speed);
    if (input.perfect_body_flips) {
        const float full_turn = rs::from_bits(0x40C90FDBu);
        const float remaining_rate = (full_turn - rs::abs(state.angle)) * 12.0f;
        const float sign = speed > 0.0f ? 1.0f : -1.0f;
        if (!(rs::abs(speed) < remaining_rate)) {
            speed = sign * remaining_rate;
        }
        state.angle = clamp(rs::mul_add(speed, rs::from_bits(0x3C888889u), state.angle), -full_turn, full_turn);
    } else {
        speed = clamp(speed, -maximum, maximum);
        state.angle = rs::mul_add(input.timestep, speed, state.angle);
    }
    state.speed = speed;
    state.spin_transform = rotation(input.normal, input.spin_angle * spin_scale);
    // Native zero-angle branch leaves the +1008 matrix untouched. `!= 0.0` is also true for a
    // NaN angle, and this matrix is *persistent*: committing one would poison every later frame's
    // reckoning system, which is how a single degenerate tick became a landing crash.
    if (state.angle != 0.0f && rs::is_finite(state.angle)) {
        const Matrix flip = rotation(input.flip_axis, state.angle);
        const Matrix spin = state.spin_transform;
        for (std::size_t col = 0; col < 4; ++col) {
            for (std::size_t lane = 0; lane < 4; ++lane) {
                const float first = col == 3 ? rs::mul_add(spin[col][0], flip[0][lane], flip[3][lane])
                                             : spin[col][0] * flip[0][lane];
                const float second = rs::mul_add(spin[col][1], flip[1][lane], first);
                state.combined_transform[col][lane] = rs::mul_add(spin[col][2], flip[2][lane], second);
            }
        }
    }
}

}  // namespace s3::air::body_flip
