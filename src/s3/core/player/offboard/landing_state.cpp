// Port of skate-core/src/player/offboard/landing_state.rs (whole file).
#include "s3/core/player/offboard/landing_state.hpp"

#include "s3/core/physics/board_motion_output.hpp"
#include "s3/core/player/offboard/hippy_jump.hpp"
#include "s3/core/player/wipeout_state/math.hpp"
#include "s3/core/player/wipeout_state/orientation.hpp"

namespace s3::player::offboard::landing_state {

using landing_deck::GRAVITY;
using landing_deck::STEP;
using landing_deck::Trajectory;
using physics::board_motion_output::inverse_length_squared;
using wipeout_state::math::wrap_angle;
using wipeout_state::orientation::projected_angle;

static const Vector UP{0.0f, 1.0f, 0.0f, 0.0f};
static const float EPSILON = rs::from_bits(0x37800000u);

void State::enter(Manager& manager, const Entry& input) {
    const bool preserve = input.previous_category == 500;
    if (!preserve) {
        manager.reset();
    }
    *this = State{};
    const Vector right = rs::map(input.animation_right, [&](float v) { return input.reversed ? -v : v; });
    transition_angle = wrap_angle(projected_angle(input.hips_up, right, UP));
    if (input.hippy || !preserve) {
        Vector velocity;
        if (input.hippy) {
            hippy_jump::Input h;
            h.desired_height = (1.0f - input.strength) * 1.35f + input.strength * 1.65f;
            h.board_position = input.board_position;
            h.centre_of_mass_position = input.com_position;
            h.reference_up = input.up;
            h.current_velocity = input.board_velocity;
            velocity = hippy_jump::calculate(h);
        } else {
            velocity = input.com_velocity;
        }
        manager.trajectory_32 = Trajectory{input.com_position, velocity, GRAVITY, -1.0f};
        manager.elapsed_160 = 0.0f;
        manager.trajectory_valid_164 = true;
        manager.force_257 = input.hippy;
        if (input.hippy) {
            takeoff_frames = 2;
        }
    } else {
        manager.elapsed_160 += STEP;
        manager.correct_trajectory(input.com_position);
    }
    hippy = input.hippy;
}

void State::align(const Settings& s, Vector board_forward, Vector animation_forward, float state_time,
                  float com_velocity_y, float input_spin) {
    const float frames = time_to_land * 59.999996f;
    float angle = projected_angle(board_forward, animation_forward, UP);
    if (state_time < 0.28f) {
        angle -= (1.0f - state_time * 3.5714285f) * transition_angle;
    }
    angle = wrap_angle(spin_rate * frames + angle);
    turning = rs::abs(angle) > rs::f32::consts::FRAC_PI_2;
    if (time_to_land > EPSILON) {
        const bool rising = com_velocity_y > 0.0f;
        const float input = rising && hippy ? input_spin : 0.0f;
        float target;
        float delta;
        if (rs::abs(input) > EPSILON) {
            target = -s.input_speed * input;
            delta = s.input_delta;
        } else {
            if (rising) {
                // 82E09C80 folds the wrapped angle modulo pi, not modulo 2pi.
                angle = wrap_angle(angle);
                if (rs::abs(angle) > rs::f32::consts::FRAC_PI_2) {
                    angle = (rs::abs(angle) - rs::f32::consts::PI) * rs::signum(angle);
                }
            }
            const float tolerance = s.minimum_auto_angle * 0.017453292f;
            if (rs::abs(angle) > tolerance) {
                const float excess = angle > 0.0f ? rs::max(angle - tolerance, 0.0f) : rs::min(angle + tolerance, 0.0f);
                target = rs::clamp(spin_rate - excess / frames, -s.automatic_speed, s.automatic_speed);
            } else {
                target = spin_rate;
            }
            delta = s.automatic_delta;
        }
        spin_rate = rs::clamp(target, spin_rate - delta, spin_rate + delta);
        applied_spin = spin_rate;
        if (state_time < 0.28f) {
            applied_spin = rs::mul_add(-transition_angle, 0.059523813f, spin_rate);
        }
    }
}

void State::advance_spin(const UpdateOutput& out) {
    accumulated_spin += applied_spin;
    half_turns = rs::as_i32(accumulated_spin * 0.31830987f);
    next_half_turns = half_turns;
    if (accumulated_spin < 0.0f && applied_spin < 0.0f) {
        next_half_turns -= 1;
    } else if (accumulated_spin > 0.0f && applied_spin > 0.0f) {
        next_half_turns += 1;
    }
    time_to_land = out.time_to_land;
    output = out;
}

float State::accurate_time(float board_y, float board_velocity_y, float com_velocity_y, std::array<float, 2> toe_y,
                           std::uint32_t wheel_contacts) {
    const float feet = (toe_y[0] + toe_y[1]) * 0.5f - 0.15000001f;
    const float velocity = com_velocity_y - board_velocity_y;
    const float displacement = board_y - feet;
    const float gravity = wheel_contacts != 0 ? -9.8f : -4.9f;
    const float discriminant = velocity * velocity + (displacement * gravity) * 2.0f;
    if (discriminant > 0.0f) {
        const float root = discriminant * inverse_length_squared(discriminant, 2);
        return (-velocity - root) / gravity;
    }
    return 0.0f;
}

void State::finish(const Settings& s, float com_velocity_y) {
    if (takeoff_frames > 0) {
        takeoff_frames -= 1;
    }
    dangerous = time_to_land < 0.07f && com_velocity_y < -s.maximum_landing_speed;
}

}  // namespace s3::player::offboard::landing_state
