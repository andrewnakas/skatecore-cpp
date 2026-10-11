// Port of skate-core/src/riding/speed_model.rs.
#include "s3/core/riding/speed_model.hpp"

#include "s3/core/riding/vector.hpp"

namespace s3::riding::speed_model {

using vector::clamp;
using vector::dot3;
using vector::normalize;

std::array<float, 8> update(SpeedModelState& state, const SpeedModelSettings& settings, const SpeedModelInput& input) {
    const float speed = rs::abs(input.signed_speed);
    const float dt = input.timestep;
    const float bound = settings.speed_error_bound;
    float lower = speed - bound;
    lower = -lower >= 0.0f ? 0.0f : lower;
    float target;
    if ((state.flags_1360 & 0x80000000u) != 0) {
        state.flags_1360 &= 0x7fffffffu;
        target = speed;
    } else {
        target = clamp(state.target_speed, lower, bound + speed);
    }
    const float coffin = (input.flags_2476 & 0x40000000u) != 0 ? settings.coffin_acceleration * dt : 0.0f;
    const float projection = dot3(input.velocity_416, input.effective_forward);
    const auto projected = rs::map(input.effective_forward, [projection](float v) { return v * projection; });
    const float normal_speed = dot3(projected, input.normal_464);
    std::array<float, 4> raw_tangent{};
    for (std::size_t i = 0; i < 4; ++i) raw_tangent[i] = projected[i] - input.normal_464[i] * normal_speed;
    const auto tangent = normalize(raw_tangent, settings.normal_threshold);
    const float gravity = clamp(-tangent[1] * settings.gravity, -settings.maximum_gravity_acceleration,
                                settings.maximum_gravity_acceleration) *
                          dt;

    if (input.frames_2580 < 30) {
        const float active = input.manual_state_276 == 0.0f ? 0.0f : 1.0f;
        float direction = input.balance > 0.0f ? 1.0f : -1.0f;
        if (0.0f > dot3(input.vector_352, tangent)) {
            direction *= -1.0f;
        }
        const float angle = rs::abs(input.angle_2652);
        const float within = angle > settings.manual_angle_limit ? 0.0f : 1.0f;
        const float negative_within = angle > settings.negative_manual_angle_limit ? 0.0f : 1.0f;
        float acceleration =
            ((((input.turn_2672 * input.turn_2672) * settings.manual_acceleration) * within) * direction) * active;
        if (acceleration < 0.0f) {
            acceleration *= negative_within;
        }
        target = acceleration + target;
    }
    const float friction = -settings.surface_friction.evaluate(input.surface_speed) * dt;
    float surface_delta;
    if (gravity > 0.0f) {
        surface_delta = friction + gravity;
    } else if (friction - gravity >= 0.0f) {
        surface_delta = gravity;
    } else {
        surface_delta = friction;
    }
    target = (surface_delta + coffin) + target;
    if (input.elapsed_without_input > settings.no_input_delay) {
        target = -rs::mul_add(settings.no_input_friction.evaluate(input.surface_speed), dt, -target);
    }
    if (input.balance != 0.0f) {
        target = -rs::mul_add(settings.manual_friction.evaluate(input.surface_speed), dt, -target);
    }
    state.target_speed = target >= 0.0f ? target : 0.0f;
    float error = clamp(state.target_speed - speed, -bound, bound);
    if (settings.override_enabled && (input.flags_2476 & 0x00400000u) != 0 && input.normal_464[1] > 0.65f) {
        float goal = settings.override_speed * rs::from_bits(0x3E8E38E4u);
        const float along = dot3(input.velocity_416, input.vector_160);
        bool all = true;
        for (float t : settings.override_direction_threshold) {
            if (!(t > along)) {
                all = false;
                break;
            }
        }
        if (all && 1.0f > input.surface_speed) {
            goal *= -1.0f;
        }
        error = clamp(goal - speed, -0.4f, 0.4f);
    }
    const bool enabled = (input.flags_2468 & 0x62010000u) == 0 && (input.flags_2476 & 0x08000000u) == 0 &&
                         input.wheel_contact_count >= 2;
    const float gain = !enabled ? 0.0f : (error < 0.0f ? settings.negative_gain : settings.positive_gain);
    const float scalar = ((input.mass * gain) * error) / dt;
    const bool forward = dot3(input.effective_forward, tangent) > 0.0f;
    std::array<float, 8> output{};
    for (std::size_t i = 0; i < 4; ++i) {
        const float axis = forward ? input.effective_forward[i] : -input.effective_forward[i];
        output[i] = axis * scalar;
    }
    return output;
}

}  // namespace s3::riding::speed_model
