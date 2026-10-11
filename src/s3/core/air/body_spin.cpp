// Port of skate-core/src/air/body_spin.rs.
#include "s3/core/air/body_spin.hpp"

namespace s3::air::body_spin {

namespace {

constexpr float STEP = rs::from_bits(0x3C888889u);  // 820849C8, fixed in these functions.

float select(float test, float nonnegative, float negative) { return test >= 0.0f ? nonnegative : negative; }

float native_clamp(float value, float lower, float upper) {
    value = select(lower - value, lower, value);
    return select(upper - value, value, upper);
}

void update_input(BodySpinState& state, float input, float auto_spin, std::uint8_t mode) {
    const float delta = input - state.get(120);
    const float derivative = rs::mul_add(delta, 1.5f, state.get(124) * 0.0f);
    state.set(164, auto_spin);
    state.set(120, input);
    state.words_[43] = (state.words_[43] & 0x00FFFFFFu) | (static_cast<std::uint32_t>(mode) << 24);
    state.set(124, derivative);
    float filtered = derivative * input > 0.1f ? derivative : 0.0f;
    filtered = native_clamp(filtered, -1.0f, 1.0f);
    state.set(128, filtered);
    const std::size_t index = state.words_[42];
    S3_ASSERT(index < 44);
    state.set(index * 4, filtered);
    state.words_[42] = static_cast<std::uint32_t>((index + 1) % 30);
}

void finish_ground(BodySpinState& state, float input) {
    state.set(152, 0.0f);
    state.set(144, 0.0f);
    state.set(140, 0.0f);
    state.set(132, rs::mul_add(state.get(132), 0.8f, input * 0.2f));
}

/// 82D8C2D8 scans the 29 entries before the current write position.
/// The newest entry starts at negative one native step, with strict peak ties.
void calculate_history_derivative(BodySpinState& state, const PointGraph<8>& graph) {
    const std::size_t end = state.words_[42];
    std::size_t index = (end + 29) % 30;
    float time = 0.0f;
    float peak = 0.0f;
    state.set(148, 0.0f);
    state.set(156, 0.0f);
    while (index != end) {
        time -= STEP;
        const float candidate = graph.evaluate(time) * state.get(index * 4);
        if (rs::abs(candidate) > rs::abs(peak)) {
            peak = candidate;
            state.set(156, time);
        }
        index = (index + 29) % 30;
    }
    state.set(148, peak);
}

void update_air(BodySpinState& state, const BodySpinSettings& settings, float input, float auto_spin,
                std::uint8_t mode) {
    const std::size_t alternate = mode == 0 ? 1 : 0;
    if (state.get(152) == 0.0f) {
        calculate_history_derivative(state, settings.curves[3 + alternate]);
    }
    const float time = state.get(152);
    const float normal_acceleration = settings.curves[alternate].evaluate(time);
    const float auto_acceleration = settings.curves[2].evaluate(time);
    const float derivative_gain = settings.curves[3 + alternate].evaluate(time);
    const float candidate = derivative_gain * state.get(128);
    if (rs::abs(candidate) > rs::abs(state.get(148))) {
        state.set(148, candidate);
        state.set(156, time);
    }
    const float next_time = time + STEP;
    state.set(152, next_time);
    // vcmpgtfp128. followed by CR6 all-lanes test, not a scalar dead zone.
    bool all = true;
    for (float v : settings.input_fade_threshold) {
        if (!(v > rs::abs(state.get(120)))) {
            all = false;
            break;
        }
    }
    if (all) {
        const float faded = state.get(132) * 0.96f;
        state.set(132, faded);
        state.set(120, faded);
    } else {
        state.set(132, rs::mul_add(state.get(132), 0.8f, input * 0.2f));
    }
    const float proportional = settings.curves[5 + alternate].evaluate(next_time) * state.get(120);
    state.set(144, proportional);
    if (proportional * state.get(148) < 0.0f) {
        state.set(148, 0.0f);
    }
    const float weight =
        rs::mul_add(rs::abs(state.get(148)), 1.0f - settings.derivative_floor, settings.derivative_floor);
    float target = -(weight * proportional);
    float acceleration = normal_acceleration;
    const float old_speed = state.get(140);
    if (rs::abs(auto_spin) > rs::from_bits(0x37800000u) &&
        (old_speed * auto_spin > 0.0f || rs::abs(old_speed) < 0.02f)) {
        acceleration = auto_acceleration;
        target = native_clamp(auto_spin, -2.0f, 2.0f);
    }
    const float limited =
        select(settings.acceleration_limit - acceleration, acceleration, settings.acceleration_limit);
    const float lower = old_speed > 0.0f ? -limited : -acceleration;
    const float upper = old_speed > 0.0f ? acceleration : limited;
    const float change = native_clamp(target - old_speed, lower, upper);
    const float speed = old_speed + change;
    state.set(140, speed);
    // Both subtractions are present at 82D8C27C..284.
    state.set(160, (target - speed) - change);
}

}  // namespace

void update(BodySpinState& state, const BodySpinSettings& settings, float input, float auto_spin,
            bool in_air, std::uint8_t mode) {
    update_input(state, input, auto_spin, mode);
    if (!in_air) {
        finish_ground(state, input);
        return;
    }
    update_air(state, settings, input, auto_spin, mode);
}

void update_ground(BodySpinState& state, float input) {
    update_input(state, input, 0.0f, 0);
    finish_ground(state, input);
}

}  // namespace s3::air::body_spin
