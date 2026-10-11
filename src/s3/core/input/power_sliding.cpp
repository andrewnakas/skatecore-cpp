// Port of skate-core/src/input/power_sliding.rs.
// Complete PowerSliding update82BB28E8 at resolved graph/component boundaries.
#include "s3/core/input/power_sliding.hpp"

#include "s3/core/input/angle.hpp"
#include "s3/core/input/controller.hpp"
#include "s3/core/physics/native_arithmetic.hpp"

namespace s3::input::power_sliding {

using rs::from_bits;
using rs::mul_add;

namespace {
void bit(std::uint32_t& word, std::uint32_t mask, bool value) { word = (word & ~mask) | (value ? mask : 0u); }
}  // namespace

float alignment(std::optional<std::tuple<std::array<float, 4>, std::array<float, 4>, std::uint8_t>> input) {
    if (!input) {
        return 1.0f;
    }
    auto [velocity, basis, flipped] = *input;
    auto dot = [](const std::array<float, 4>& a, const std::array<float, 4>& b) {
        return physics::native_arithmetic::dot3(a, b);
    };
    const float squared = dot(velocity, velocity);
    if (!(squared > from_bits(0x3a83126fu))) {
        return 1.0f;
    }
    const float length = controller::magnitude(squared);
    float reciprocal = angle::reciprocal_estimate(length);
    for (int i = 0; i < 2; ++i) {
        reciprocal = mul_add(reciprocal, mul_add(-reciprocal, length, 1.0f), reciprocal);
    }
    velocity = rs::map(velocity, [reciprocal](float v) { return reciprocal * v; });
    if (flipped != 0) {
        basis = rs::map(basis, [](float v) { return -v; });
    }
    return dot(velocity, basis);
}

void update(State& state, std::array<std::uint32_t, 5>& latch, Input input, const Settings& settings,
            const std::function<float()>& clock) {
    const float right = input.right_slide ? *input.right_slide : 0.0f;
    const float left = input.left_slide ? *input.left_slide : 0.0f;
    const bool active = input.category == 1;
    if (active && (state.flags & 0x20000000u) == 0) {
        state.elapsed = 0.0f;
        state.previous_right = right;
        state.previous_left = left;
        state.flags &= 0x3fffffffu;
    }
    bit(state.flags, 0x20000000u, active);
    if (!active) {
        return;
    }
    const bool fast = input.speed > settings.minimum_speed;
    const bool start_right =
        fast && input.right_query && (state.flags & 0x80000000u) == 0 && state.previous_right < right;
    bit(latch[3], 0x80000000u, start_right);
    state.previous_right = right;
    bit(state.flags, 0x80000000u, input.right_query);
    const bool start_left =
        fast && input.left_query && (state.flags & 0x40000000u) == 0 && state.previous_left > left;
    bit(latch[1], 0x80000000u, start_left);
    state.previous_left = left;
    bit(state.flags, 0x40000000u, input.left_query);
    const float stop_time = settings.stop_time.evaluate(input.graph_scalar);
    const float response =
        settings.speed_response.evaluate(input.speed) * (1.0f - rs::abs(input.graph_scalar));
    const float threshold = settings.angle_response.evaluate(input.graph_scalar);
    if (response < threshold) {
        const float elapsed = state.elapsed;
        state.elapsed = elapsed + clock();
    } else {
        state.elapsed = 0.0f;
    }
    const bool stop = state.elapsed > stop_time;
    const float right_time = clock() + from_bits(latch[2]);
    latch[2] = rs::to_bits(right_time);
    const float left_time = clock() + from_bits(latch[0]);
    latch[0] = rs::to_bits(left_time);
    const bool exit_right =
        right_time > settings.minimum_slide_time && (!input.right_slide.has_value() || !fast || stop);
    const bool exit_left =
        left_time > settings.minimum_slide_time && (!input.left_slide.has_value() || !fast || stop);
    bit(latch[3], 0x40000000u, exit_right);
    bit(latch[1], 0x40000000u, exit_left);
}

}  // namespace s3::input::power_sliding
