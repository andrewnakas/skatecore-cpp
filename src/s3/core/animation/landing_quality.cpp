// Port of skate-core/src/animation/landing_quality.rs.
#include "s3/core/animation/landing_quality.hpp"

#include "s3/core/physics/board_motion_output.hpp"
#include "s3/core/physics/native_arithmetic.hpp"

namespace s3::animation::landing_quality {

namespace {
using V = std::array<float, 4>;
float dot(const V& a, const V& b) { return physics::native_arithmetic::dot3(a, b); }
V scale(const V& a, float b) { return rs::map(a, [b](float x) { return x * b; }); }
V planar(const V& a, const V& normal) {
    const V projected = scale(normal, dot(normal, a));
    return rs::from_fn<4>([&](std::size_t i) { return a[i] - projected[i]; });
}
V cross(const V& a, const V& b) {
    return {rs::mul_add(-a[2], b[1], a[1] * b[2]), rs::mul_add(-a[0], b[2], a[2] * b[0]),
            rs::mul_add(-a[1], b[0], a[0] * b[1]), 0.0f};
}
float length(const V& a) {
    const float square = dot(a, a);
    const float value = square * physics::board_motion_output::inverse_length_squared(square, 2);
    return square == 0.0f ? 0.0f : value;
}
float reciprocal(float value) {
    float estimate = physics::native_arithmetic::reciprocal_estimate(value);
    for (int i = 0; i < 2; ++i) {
        estimate = rs::mul_add(estimate, rs::mul_add(-estimate, value, 1.0f), estimate);
    }
    return estimate;
}
float upper_one(float value) { return 1.0f - value >= -0.0f ? value : 1.0f; }
}  // namespace

void Output::update(Input input, const Settings& settings) {
    if (input.previous_filtered_state == 1 || input.filtered_state != 1) {
        return;
    }
    const V velocity = planar(input.deck_velocity, input.ground_normal);
    const V forward = planar(input.reckoning_forward, input.ground_normal);
    const float speed = length(velocity);
    const float forward_length = length(forward);
    float forward_speed = rs::abs(dot(velocity, forward));
    float side_speed = rs::abs(dot(cross(forward, input.ground_normal), input.deck_velocity));
    std::uint32_t kind = 0;
    float spin = 0.0f;
    if (forward_length * speed < rs::from_bits(0x3727c5ac)) {
        forward_speed = 0.0f;
        side_speed = 0.0f;
    } else {
        const V normalized_velocity = scale(velocity, reciprocal(speed));
        const V normalized_forward = scale(forward, reciprocal(forward_length));
        float orientation = cross(normalized_forward, normalized_velocity)[1];
        if (input.flipped) {
            orientation = -orientation;
        }
        const float angular_speed = -input.air_spin;
        const float spin_input = (rs::abs(angular_speed) - 0.1f) * 0.14492753f;
        const float spin_curve = settings.twist_spin.evaluate(upper_one(spin_input));
        const float side_input = (side_speed - 0.1f) * 0.1010101f;
        const float side_curve = settings.side_speed.evaluate(upper_one(side_input));
        if (speed >= 2.0f) {
            const V heading = input.flipped ? scale(normalized_forward, -1.0f) : normalized_forward;
            const bool toward = dot(heading, normalized_velocity) >= 0.0f;
            float magnitude;
            if (rs::abs(angular_speed) <= 0.1f) {
                kind = 1;
                magnitude = side_curve;
            } else {
                const float product = angular_speed * orientation;
                kind = ((toward && product <= 0.0f) || (!toward && product > 0.0f)) ? 2 : 1;
                magnitude = spin_curve - side_curve >= -0.0f ? spin_curve : side_curve;
            }
            spin = angular_speed >= -0.0f ? magnitude : -magnitude;
            if (rs::abs(orientation) < 0.2f && (kind != 2 || !(rs::abs(spin) > 0.3f))) {
                kind = rs::abs(orientation) >= 0.05f ? 3 : 0;
                spin = 0.0f;
            }
        }
    }
    *this = Output{spin, side_speed, forward_speed, spin, kind, true};
}

}  // namespace s3::animation::landing_quality
