// Port of skate-core/src/riding/ground_orientation.rs.
#include "s3/core/riding/ground_orientation.hpp"

#include "s3/core/physics/board_ground.hpp"
#include "s3/core/physics/board_motion_output.hpp"
#include "s3/core/physics/native_arithmetic.hpp"

namespace s3::riding::ground_orientation {

using physics::board_ground::angle_between;
using physics::board_motion_output::add;
using physics::board_motion_output::dot;
using physics::board_motion_output::inverse_length_squared;
using physics::board_motion_output::length;
using physics::board_motion_output::scale;
using physics::board_motion_output::subtract;
namespace native_arithmetic = physics::native_arithmetic;

namespace {

std::array<float, 4> lanes(Vector3 v) { return {v.x, v.y, v.z, 0.0f}; }
Vector3 vector(std::array<float, 4> v) { return Vector3{v[0], v[1], v[2]}; }
float fsel(float test, float positive, float negative) { return test >= -0.0f ? positive : negative; }
float unit_saturate(float value) {
    value = fsel(-value, 0.0f, value);
    return fsel(1.0f - value, value, 1.0f);
}
Vector3 madd(Vector3 v, float factor, Vector3 offset) {
    return Vector3{rs::mul_add(v.x, factor, offset.x), rs::mul_add(v.y, factor, offset.y),
                   rs::mul_add(v.z, factor, offset.z)};
}
Vector3 blend(Vector3 from, Vector3 to, float amount) { return madd(to, amount, scale(from, 1.0f - amount)); }
Vector3 cross(Vector3 a, Vector3 b) {
    return Vector3{rs::mul_add(-a.z, b.y, a.y * b.z), rs::mul_add(-a.x, b.z, a.z * b.x),
                   rs::mul_add(-a.y, b.x, a.x * b.y)};
}
Vector3 normalize_safe(Vector3 value, Vector3 fallback) {
    const float squared = dot(value, value);
    const float inverse = inverse_length_squared(squared, 2);
    const float magnitude = squared == 0.0f ? 0.0f : squared * inverse;
    if (magnitude > EPSILON) {
        return scale(value, inverse);
    }
    return fallback;
}

}  // namespace

GroundOrientation GroundOrientation::new_(const GroundOrientationSettings& settings) {
    return GroundOrientation{
        UP,
        UP,
        UP,
        Vector3::ZERO,
        UP,
        0.0f,
        GroundNormalFilter::initialized(settings.ground_normal_smoothing, lanes(UP)),
        GroundNormalFilter::initialized(settings.up_vector_smoothing_slow, lanes(UP)),
        GroundNormalFilter::initialized(settings.up_vector_smoothing_fast, lanes(UP)),
    };
}

void GroundOrientation::reset() {
    dynamic_up = UP;
    up = UP;
    target = UP;
    up_velocity = Vector3::ZERO;
    ground_normal = UP;
    ground_blend = 0.0f;
}

void GroundOrientation::update(const GroundOrientationSettings& settings, GroundOrientationInput input) {
    const Vector3 normalized_com = normalize_safe(input.com_to_deck, Vector3::ZERO);
    const float amount = settings.dynamic_up_vs_ground_y.evaluate(input.ground_normal.y);
    const Vector3 prediction = normalize_safe(
        blend(ground_normal, add(UP, subtract(input.dynamic_up, normalized_com)), amount), Vector3::ZERO);
    dynamic_up = input.dynamic_up;
    ground_normal = vector(ground_filter.update(settings.ground_normal_smoothing, lanes(input.ground_normal)));
    const Vector3 horizontal_axis = cross(UP, ground_normal);
    if (dot(horizontal_axis, horizontal_axis) > rs::from_bits(0x3A83126Fu)) {
        const Vector3 axis =
            scale(horizontal_axis, inverse_length_squared(dot(horizontal_axis, horizontal_axis), 2));
        const Vector3 projected = subtract(prediction, scale(axis, dot(axis, prediction)));
        const float prediction_to_ground = angle_between(projected, ground_normal);
        const float prediction_to_up = angle_between(projected, UP);
        const float ground_to_up = angle_between(ground_normal, UP);
        const float old_up_to_up = angle_between(up, UP);
        Vector3 candidate = prediction_to_ground < prediction_to_up ? ground_normal : UP;
        if (prediction_to_ground < ground_to_up && prediction_to_up < ground_to_up) {
            candidate = projected;
        }
        const float alpha = (input.speed + 1.0f) * rs::from_bits(0x3A83126Fu);
        if (!(old_up_to_up > ground_to_up || prediction_to_up > old_up_to_up)) {
            candidate = madd(up, 1.0f - alpha, scale(candidate, alpha));
        }
        const float slope = unit_saturate(ground_to_up * rs::from_bits(0x3EA2F983u));
        float target_blend = settings.ground_vector_blend.evaluate(slope);
        if (input.wheel_contact_count <= settings.minimum_wheels_for_ground_blend) {
            target_blend = 0.0f;
        }
        const float delta = target_blend - ground_blend;
        const float lower =
            fsel(-settings.ground_blend_max_delta - delta, -settings.ground_blend_max_delta, delta);
        ground_blend += fsel(settings.ground_blend_max_delta - lower, lower, settings.ground_blend_max_delta);
        target = normalize_safe(blend(candidate, ground_normal, ground_blend), Vector3::ZERO);
    } else {
        target = ground_normal;
    }
    if (input.wheel_contact_count >= 2 && input.animation_balance == 0.0f) {
        const float usage = settings.deck_angle_usage_vs_speed.evaluate(input.deck_angle_curve_input);
        const Vector3 axis = normalize_safe(cross(input.board_forward, ground_normal), Vector3::ZERO);
        const Vector3 deck_up =
            normalize_safe(subtract(input.board_up, scale(axis, dot(input.board_up, axis))), Vector3::ZERO);
        target = normalize_safe(blend(target, deck_up, usage), Vector3::ZERO);
    }
    const Vector3 slow =
        normalize_safe(vector(slow_filter.filter_raw(settings.up_vector_smoothing_slow, lanes(target))),
                       Vector3::ZERO);
    const Vector3 fast =
        normalize_safe(vector(fast_filter.filter_raw(settings.up_vector_smoothing_fast, lanes(target))),
                       Vector3::ZERO);
    const float speed_fraction = input.speed * rs::from_bits(0x3DCCCCCDu);
    const float mix = settings.up_vector_smoothing_vs_speed.evaluate(speed_fraction);
    const Vector3 filtered = normalize_safe(blend(slow, fast, mix), Vector3::ZERO);
    const float max_delta =
        settings.up_vector_max_delta_vs_speed.evaluate(speed_fraction) * rs::from_bits(0x3E4CCCCDu);
    const Vector3 difference = subtract(filtered, up);
    const float magnitude = length(difference);
    const float bounded = fsel(magnitude - max_delta, max_delta, magnitude);
    const Vector3 stepped =
        normalize_safe(madd(normalize_safe(difference, Vector3::ZERO), bounded, up), Vector3::ZERO);
    const Vector3 desired_velocity = subtract(stepped, up);
    const Vector3 acceleration = clamp_length(subtract(desired_velocity, up_velocity),
                                              settings.up_vector_max_acceleration * rs::from_bits(0x3C888889u));
    up_velocity = add(up_velocity, acceleration);
    const Vector3 side = scale(input.previous_reckoning_right, dot(up_velocity, input.previous_reckoning_right));
    up_velocity = madd(side, settings.extra_side_damping, subtract(up_velocity, side));
    const Vector3 unnormalized = add(up, up_velocity);
    up = normalize_safe(unnormalized, unnormalized);
    if (0.0f > dot(up_velocity, desired_velocity)) {
        up_velocity = scale(up_velocity, settings.anti_wobble_damping);
    }
    if (input.prevent_up_behind_board && dot(up, input.effective_board_forward) < 0.0f) {
        const Vector3 side_ = cross(up, input.effective_board_forward);
        up = normalize_safe(cross(input.effective_board_forward, side_), up);
        up_velocity = scale(up_velocity, 0.5f);
    }
    slow_filter.publish_current(lanes(up));
    fast_filter.publish_current(lanes(up));
}

Vector3 clamp_length(Vector3 value, float maximum) {
    const float magnitude = length(value);
    if (magnitude < rs::from_bits(0x37800000u)) {
        return value;
    }
    const float bounded = fsel(maximum - magnitude, magnitude, maximum);
    float inverse = native_arithmetic::reciprocal_estimate(magnitude);
    for (int i = 0; i < 2; ++i) {
        const float error = rs::mul_add(-inverse, magnitude, 1.0f);
        inverse = rs::mul_add(inverse, error, inverse);
    }
    return scale(scale(value, bounded), inverse);
}

}  // namespace s3::riding::ground_orientation
