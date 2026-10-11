// Port of skate-core/src/riding/heading.rs.
#include "s3/core/riding/heading.hpp"

#include "s3/core/physics/reciprocal_sqrt.hpp"
#include "s3/core/riding/vector.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::riding::heading {

namespace {

float clamp(float value, float lower, float upper) {
    const float v = lower - value >= 0.0f ? lower : value;
    return upper - v >= 0.0f ? v : upper;
}

float dot3(const std::array<float, 4>& a, const std::array<float, 4>& b) { return vector::dot3(a, b); }

std::array<float, 4> scaled(const std::array<float, 4>& v, float s) {
    return rs::map(v, [s](float n) { return n * s; });
}

}  // namespace

std::array<float, 4> calculate(const HeadingSettings& settings, const HeadingInput& input, float& previous) {
    if (input.balance != 0.0f && input.manual_turn != 0.0f) {
        const bool wrong = ((input.flags_2472 & 0x08000000u) != 0 && input.balance < 0.0f) ||
                           ((input.flags_2472 & 0x04000000u) != 0 && input.balance > 0.0f);
        const float turn = wrong ? input.manual_turn * settings.manual_wrong_wheel_scalar : input.manual_turn;
        const float target = settings.manual_response.evaluate(input.manual_curve_input) * rs::from_bits(0x3C8EFA35u);
        previous = rs::mul_add(1.0f - settings.manual_damping, previous, target * settings.manual_damping);
        const float scalar = (input.timestep * previous) * turn;
        return scaled(input.normal, scalar);
    }
    previous = 0.0f;
    const auto& matrix = input.transform;
    std::array<std::array<float, 4>, 3> rows{};
    for (std::size_t i = 0; i < 3; ++i) rows[i] = {matrix[0][i], matrix[1][i], matrix[2][i], 0.0f};
    const float cosine = clamp(rows[1][1], -1.0f, 1.0f);
    float angle = trigonometry::acos(cosine) * rs::from_bits(0x3F22F983u);
    angle = angle > 1.2f ? 0.0f : angle;
    std::array<float, 4> downhill{};
    for (std::size_t i = 0; i < 4; ++i) {
        float v = rows[0][i] * 0.0f;
        v = rs::mul_add(rows[1][i], -1.0f, v);
        downhill[i] = rs::mul_add(rows[2][i], 0.0f, v);
    }
    downhill[1] = 0.0f;
    const float squared = dot3(downhill, downhill);
    float inverse = physics::reciprocal_sqrt::estimate(squared);
    for (int n = 0; n < 2; ++n) {
        const float correction = rs::mul_add(-squared, inverse * inverse, 1.0f);
        inverse = rs::mul_add(inverse * 0.5f, correction, inverse);
    }
    const float length = squared == 0.0f ? 0.0f : squared * inverse;
    const float inclination = settings.inclination_response.evaluate(angle);
    for (std::size_t i = 0; i < 4; ++i) {
        downhill[i] = (length > settings.normal_threshold[i] ? downhill[i] * inverse : 0.0f) * inclination;
    }
    std::array<float, 4> velocity{};
    for (std::size_t i = 0; i < 4; ++i) {
        float v = rows[0][i] * input.velocity[0];
        v = rs::mul_add(rows[1][i], input.velocity[1], v);
        velocity[i] = rs::mul_add(rows[2][i], input.velocity[2], v);
    }
    velocity[1] = 0.0f;
    // Native cyclic permutations select the Y component of velocity x downhill.
    const float cross_y = rs::mul_add(-velocity[0], downhill[2], velocity[2] * downhill[0]);
    const float speed = clamp(rs::abs(input.signed_speed), 0.0f, settings.speed_max);
    const float heading =
        (cross_y * settings.heading_strength) * settings.speed_response.evaluate(speed / settings.speed_max);
    float opposing = dot3(input.angular_velocity, input.normal) * -input.turn_2712;
    opposing = opposing >= 0.0f ? opposing : 0.0f;
    const float turn = ((settings.angular_response.evaluate(opposing) * input.scalar_2740) * settings.turn_strength) *
                       input.turn_2712;
    const float scalar = !(heading * turn < 0.0f) && !(rs::abs(turn) > rs::abs(heading)) ? heading : turn;
    return scaled(input.normal, scalar);
}

}  // namespace s3::riding::heading
