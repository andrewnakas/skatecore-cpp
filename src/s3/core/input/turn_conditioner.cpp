// Port of skate-core/src/input/turn_conditioner.rs.
// Complete steering conditioner 82DEFF38, called by82DEFB30.
#include "s3/core/input/turn_conditioner.hpp"

namespace s3::input::turn_conditioner {

using rs::mul_add;

namespace {

float clamp(float value, float low, float high) {
    value = low - value >= 0.0f ? low : value;
    return high - value >= 0.0f ? value : high;
}

float filter(std::array<float, 9>& state, const std::array<float, 4>& coefficients, float input) {
    for (std::size_t i = 0; i < 4; ++i) state[i] = coefficients[i];
    const float difference = input - state[4];
    const float input_difference = input - state[5];
    const float complement = 1.0f - state[3];
    state[5] = input;
    state[8] = mul_add(input_difference, state[3], state[8] * complement);
    state[6] = mul_add(difference, state[3], complement * state[6]);
    float term = mul_add(state[8] - state[7], state[2], state[1] * state[6]);
    term = mul_add(difference, state[0], term);
    const float result = term + state[4];
    state[7] = result - state[4];
    state[4] = result;
    return result;
}

}  // namespace

Output update(State& state, Input input, const Settings& settings) {
    const auto& p = settings.parameters;
    const float sign = input.animation_156 != 0 ? -1.0f : 1.0f;
    const float scaled = input.bundle_32_field_264 * sign;
    const float speed_fraction = clamp(input.body_160 / p[11], 0.0f, 1.0f);
    const float other_fraction = clamp((input.bundle_36_field_160 / p[10]) * sign, -1.0f, 1.0f);
    const float base = settings.input_curve.evaluate(speed_fraction) * scaled;
    const float attenuated = mul_add(-(1.0f - p[8]), input.body_176, 1.0f) * other_fraction;
    const float target = mul_add(attenuated, p[9], (1.0f - p[9]) * base);
    float first = filter(state.filters[0], settings.filter_coefficients[0], target);
    const float epsilon = rs::from_bits(0x3c23d70au);
    first = rs::abs(first) > epsilon ? first : 0.0f;
    float second = filter(state.filters[1], settings.filter_coefficients[1], target);
    second = rs::abs(second) > epsilon ? second : 0.0f;
    const float third = filter(state.filters[2], settings.filter_coefficients[2], (target - second) / p[12]);
    float movement = input.animation_152 != 0 ? scaled : 0.0f;
    float difference = movement - state.history[0];
    state.history[0] = movement;
    state.history[2] = mul_add(rs::abs(difference), p[2], (1.0f - p[2]) * state.history[2]);
    const float ratio = state.history[2] / p[1];
    const float squared = ratio * ratio;
    const float bounded = squared - 1.0f >= 0.0f ? 1.0f : squared;
    state.history[3] = clamp(bounded, state.history[3] - p[0], state.history[3] + p[0]);
    float quick = settings.quickness_curve.evaluate(state.history[3]);
    quick = clamp(quick, 0.0f, 1.0f);
    quick = settings.speed_curve.evaluate(input.body_160) * quick;
    movement = input.animation_152 != 0 ? second : 0.0f;
    difference = movement - state.history[1];
    state.history[1] = movement;
    state.history[5] = mul_add(rs::abs(difference), p[5], (1.0f - p[5]) * state.history[5]);
    const float hold = state.history[5] < p[3] ? 1.0f : 0.0f;
    float alpha = mul_add(1.0f - hold, p[7], p[6] * hold);
    const float next = mul_add(1.0f - alpha, state.history[4], alpha * hold);
    difference = next - state.history[4];
    state.history[6] = clamp(difference, state.history[6] - p[4], state.history[6] + p[4]);
    state.history[4] = clamp(state.history[4] + state.history[6], 0.0f, 1.0f);
    alpha = settings.smoothing_curve.evaluate(input.body_176);
    state.history[7] = mul_add(1.0f - alpha, state.history[7], alpha * first);
    const float turn = clamp(state.history[7], -1.0f, 1.0f);
    return {
        turn,
        clamp(target, -1.0f, 1.0f),
        base,
        turn * sign,
        base * sign,
        quick,
        clamp(third, -1.0f, 1.0f),
        clamp(state.history[4], 0.0f, 1.0f),
    };
}

}  // namespace s3::input::turn_conditioner
