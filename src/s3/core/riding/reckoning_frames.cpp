// Port of skate-core/src/riding/reckoning_frames.rs.
#include "s3/core/riding/reckoning_frames.hpp"

#include "s3/core/math.hpp"
#include "s3/core/physics/board_motion_output.hpp"
#include "s3/core/physics/native_arithmetic.hpp"
#include "s3/core/riding/collision_response.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::riding::reckoning_frames {

using math::Vector3;
using physics::board_motion_output::inverse_length_squared;
using physics::native_arithmetic::dot3;
using physics::skeleton_animation_record::compose_affine;
using riding::collision_response::signed_angle;
using V4 = std::array<float, 4>;

namespace {

V4 cross(const V4& a, const V4& b) {
    return V4{
        rs::mul_add(-a[2], b[1], a[1] * b[2]),
        rs::mul_add(-a[0], b[2], a[2] * b[0]),
        rs::mul_add(-a[1], b[0], a[0] * b[1]),
        rs::mul_add(-a[3], b[3], a[3] * b[3]),
    };
}
AnimationPartTransform inverse(const AnimationPartTransform& frame) {
    std::array<V4, 3> axes{};
    for (std::size_t i = 0; i < 3; ++i) axes[i] = V4{frame[0][i], frame[1][i], frame[2][i], 0.0f};
    V4 p{};
    for (std::size_t i = 0; i < 4; ++i) p[i] = 0.0f - frame[3][i];
    V4 position{};
    for (std::size_t i = 0; i < 4; ++i) {
        const float z = axes[2][i] * p[2];
        const float y = rs::mul_add(axes[1][i], p[1], z);
        position[i] = rs::mul_add(axes[0][i], p[0], y);
    }
    return AnimationPartTransform{axes[0], axes[1], axes[2], position};
}
float wrap_fraction(float angle) {
    const float turns = angle * rs::from_bits(0x3e22f983u);
    float fraction = turns - rs::floor(turns);
    fraction = fraction - (fraction > 0.5f ? 1.0f : 0.0f);
    return fraction * rs::from_bits(0x40c90fdbu);
}
Vector3 xyz(const V4& v) { return Vector3{v[0], v[1], v[2]}; }

}  // namespace

V4 normalize(V4 value) {
    const float reciprocal = inverse_length_squared(dot3(value, value), 2);
    return V4{value[0] * reciprocal, value[1] * reciprocal, value[2] * reciprocal, value[3] * reciprocal};
}

V4 normalize_or(V4 value, V4 fallback) {
    const float squared = dot3(value, value);
    if (squared > 0.0f && rs::is_finite(squared)) {
        const float reciprocal = inverse_length_squared(squared, 2);
        const V4 scaled{value[0] * reciprocal, value[1] * reciprocal, value[2] * reciprocal,
                        value[3] * reciprocal};
        bool all_finite = true;
        for (float v : scaled) all_finite = all_finite && rs::is_finite(v);
        if (all_finite) {
            return scaled;
        }
    }
    return fallback;
}

void ReckoningFrames::calculate_transform(V4 up, V4 ground_normal) {
    // `up` parallel to `heading` -- a vertical board, which University's
    // walls produce readily -- collapses `right` and then `forward` to zero.
    // `normalize` has no epsilon fallback in this leaf, and
    // `inverse_length_squared(0, 2)` is NaN rather than infinity (the first
    // Newton step computes `fma(-0, inf, 1)`). `heading` is *persistent*, so
    // one degenerate frame poisons it for the rest of the session: that NaN
    // reaches `animation_to_world`, then the deck's straighten/heading
    // displacements, and lands as "Non-finite torque_acceleration before
    // shared solve" on some later touchdown -- the crash the owner sees.
    //
    // Retaining the previous axis is the standard degenerate handling and is
    // what the off-board frames already do (`surface_frame::math::normalize_or`).
    // Every non-degenerate input keeps its exact former value.
    const V4 right = cross(up, heading);
    const V4 forward = cross(right, up);
    heading = normalize_or(forward, heading);
    system[0] = normalize_or(right, system[0]);
    system[1] = up;
    system[2] = heading;
    unflipped = system;
    system = compose_affine(body_flip, system);
    inverse_system = inverse(system);

    const V4 ground_right = cross(ground_normal, heading);
    const V4 ground_forward = cross(ground_right, ground_normal);
    ground[0] = normalize_or(ground_right, ground[0]);
    ground[1] = ground_normal;
    ground[2] = normalize_or(ground_forward, ground[2]);
}

void ReckoningFrames::calculate_dynamic_lean(V4 up, V4 dynamic_up) {
    const V4 axis = system[2];
    auto project = [&](const V4& value) {
        const float amount = dot3(value, axis);
        V4 out{};
        for (std::size_t i = 0; i < 4; ++i) out[i] = value[i] - axis[i] * amount;
        return out;
    };
    const V4 from = project(up);
    const V4 to = project(dynamic_up);
    const float threshold = rs::from_bits(0x3727c5acu);  // 8219B100
    target_lean_angle = dot3(from, from) > threshold && dot3(to, to) > threshold
                            ? wrap_fraction(signed_angle(xyz(from), xyz(to), xyz(axis)))
                            : 0.0f;
}

void ReckoningFrames::calculate_tilt(bool reverse_stance, const PointGraph<8>& tilt_vs_angle,
                                     const PointGraph<8>& tilt_vs_up) {
    const V4 axis = unflipped[1];
    const V4 world_up{0.0f, 1.0f, 0.0f, 0.0f};
    const float dot = dot3(axis, world_up);
    V4 parallel{};
    for (std::size_t i = 0; i < 4; ++i) parallel[i] = axis[i] * dot;
    V4 projected{};
    for (std::size_t i = 0; i < 4; ++i) projected[i] = world_up[i] - parallel[i];
    const float squared = dot3(projected, projected);
    if (squared <= rs::from_bits(0x3a03126fu)) {
        return;
    }
    const float angle =
        world_up == parallel ? 0.0f : signed_angle(xyz(unflipped[2]), xyz(normalize(projected)), xyz(axis));
    const float half_pi = rs::from_bits(0x3fc90fdbu);
    const float scale = rs::from_bits(0x3f22f983u);
    float normalized_angle = ((rs::abs(wrap_fraction(angle - half_pi)) - half_pi) * -1.0f) * scale;
    if (reverse_stance) {
        normalized_angle = -normalized_angle;
    }
    // Inline native asin polynomial82D8C704..7E0 is the same operation
    // tree retained by trigonometry::asin, including one rsqrt refinement.
    const float normalized_up_angle = (half_pi - trigonometry::asin(axis[1])) * scale;
    const float angle_tilt = tilt_vs_angle.evaluate(rs::abs(normalized_angle));
    const float up_tilt = tilt_vs_up.evaluate(rs::abs(normalized_up_angle));
    const float signed_ = normalized_angle >= -0.0f ? angle_tilt : -angle_tilt;
    lateral_tilt = V4{up_tilt * signed_, 0.0f, 0.0f, 0.0f};
}

}  // namespace s3::riding::reckoning_frames
