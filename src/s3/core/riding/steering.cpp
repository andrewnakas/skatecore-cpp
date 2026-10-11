// Port of skate-core/src/riding/steering.rs.
#include "s3/core/riding/steering.hpp"

#include <utility>

namespace s3::riding::steering {

float calculate_tilt(const SteeringSettings& settings, SteeringInput input, float* push_scalar,
                     float* damped_turn) {
    const float hard_scalar = rs::mul_add(rs::abs(input.hard_turn), settings.hard_turn_increase, 1.0f);
    float turn = input.turn;
    if (input.hard_turn != 0.0f) {
        turn = turn >= 0.0f ? 1.0f : -1.0f;
    }
    if (damped_turn != nullptr) {
        turn = rs::mul_add(1.0f - settings.damping, *damped_turn, settings.damping * turn);
        *damped_turn = turn;
    }
    const float speed_scalar = settings.speed_graph.evaluate(
        rs::clamp(input.absolute_body_speed / settings.speed_graph_max_speed, 0.0f, 1.0f));
    const float input_scalar = settings.input_graph.evaluate(rs::abs(turn));
    float push = 1.0f;
    if (push_scalar != nullptr) {
        const float next = input.pushing ? *push_scalar - settings.push_scalar_decrement
                                         : settings.push_scalar_increment + *push_scalar;
        *push_scalar = rs::clamp(next, settings.push_scalar_min, 1.0f);
        push = *push_scalar;
    }
    const float manual = input.balance != 0.0f ? settings.manual_scalar : 1.0f;
    const float tightness =
        rs::mul_add(settings.tight_trucks_scalar, input.truck_tightness, 1.0f) - input.truck_tightness;
    return input.flipped_controls_scalar * settings.general_scalar * manual * tightness * push * input_scalar *
           speed_scalar * hard_scalar * turn;
}

void TruckSteeringState::update(float target, float blend, std::uint32_t flags_2468, std::uint32_t flags_2472) {
    deck_tilt = rs::mul_add(1.0f - blend, deck_tilt, blend * target);
    std::array<bool, 2> active{(flags_2472 & (1u << 27)) != 0, (flags_2472 & (1u << 26)) != 0};
    if ((flags_2468 & (1u << 20)) != 0) {
        std::swap(active[0], active[1]);
    }
    for (std::size_t index = 0; index < 2; ++index) {
        float& timer = activation_time[index];
        float& angle = targets[index];
        if (active[index]) {
            if (timer <= 0.16500001f) {
                timer += rs::from_bits(0x3C888889u);
                const float fraction = rs::clamp(timer * 6.060606f, 0.0f, 1.0f);
                angle = rs::mul_add(1.0f - fraction, angle, fraction * deck_tilt);
            } else {
                angle = deck_tilt;
            }
        } else {
            // The first inactive frame holds the angle; subsequent frames decay.
            if (timer == 0.0f) {
                angle *= 0.983f;
            }
            timer = 0.0f;
        }
    }
}

}  // namespace s3::riding::steering
