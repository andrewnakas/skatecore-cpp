// Port of skate-core/src/air/known/body.rs.
#include "s3/core/air/known/body.hpp"

#include "s3/core/air/known/trajectory.hpp"

namespace s3::air::known::body {

using trajectory::select;

namespace {
constexpr float DEGREES_TO_RADIANS = rs::from_bits(0x3C8EFA35u);  // 0x8206D110.
constexpr float RADIANS_TO_DEGREES = rs::from_bits(0x42652EE1u);  // 0x82084620.
constexpr float DIRECTION_EPSILON = rs::from_bits(0x3A83126Fu);   // 0x82063A48.
constexpr float PI = rs::from_bits(0x40490FDBu);                  // 0x82060C44.
constexpr float ALIGNMENT_DELAY = rs::from_bits(0x3D03126Fu);     // 0x822F91E8.
constexpr float AUTO_SPIN_SCALE = rs::from_bits(0x3F99999Au);     // 0x821659F4.
constexpr float FORTY_FIVE_DEGREES = rs::from_bits(0x42340000u);  // 0x82099018.
constexpr float HALF = rs::from_bits(0x3F000000u);                // 0x8209975C.
constexpr float REVERSE_SCALE = rs::from_bits(0x3BA3D70Au);       // 0x82099920.
constexpr float DECELERATION_SCALE = rs::from_bits(0x3F400000u);  // 0x821814A0.

Vector4 flip_sign_bits(Vector4 vector) {
    Vector4 out{};
    for (std::size_t lane = 0; lane < 4; ++lane) out[lane] = rs::from_bits(rs::to_bits(vector[lane]) ^ 0x80000000u);
    return out;
}

float native_clamp(float value, float lower, float upper) {
    const float v = select(lower - value, lower, value);
    return select(upper - v, v, upper);
}
}  // namespace

float calculate_body_flip_speed(KnownAirState& state, const KnownAirFrame& frame, const KnownAirSettings& settings,
                                const KnownAirModeSettings& mode, const KnownAirReckoningFields& reckoning,
                                KnownAirRuntime& runtime) {
    if (!state.body_flipping_211) {
        const std::uint32_t flags = frame.flags_2468;
        const bool has_flip = (flags & 0x00000020u) != 0 || (flags & 0x00000010u) != 0;
        const bool has_grab = (flags & 0x00000080u) != 0 || (flags & 0x00000040u) != 0;
        if (has_flip && has_grab && (frame.flags_2488 & 0x02000000u) == 0) {
            const float threshold =
                settings.flip_start_collision_time_vs_normal_y.evaluate(reckoning.landing_normal_1152[1]);
            if (state.collision_time_196 > threshold) {
                runtime.begin_reckoning_body_flip(((flags >> 7) & 1u) != 0);
                state.body_flipping_211 = true;
            }
        }
    }

    if (mode.perfect_body_flips_28) {
        return state.body_flip_target_speed_204;
    }
    const std::uint32_t flags = frame.flags_2468;
    if ((flags & 0x00000020u) != 0 || (flags & 0x00000010u) != 0 ||
        state.time_in_state_180 < settings.body_flip_min_grab_time_fraction_456) {
        return state.body_flip_target_speed_204;
    }
    return 0.0f;
}

float calculate_body_spin_speed(const KnownAirState& state, const KnownAirFrame& frame,
                                const KnownAirSettings& settings, const KnownAirModeSettings& mode,
                                const KnownAirReckoningFields& reckoning, KnownAirRuntime& runtime) {
    const float scaled_input = frame.body_spin_input_2640 * settings.max_spin_speed_428;
    const float manual_spin = scaled_input * DEGREES_TO_RADIANS;
    float result = (frame.flags_2472 & 0x10000000u) != 0 ? manual_spin : 0.0f;

    // Native tests the unmasked manual value, not `result`.
    if (manual_spin == 0.0f && state.landing_heading_valid_209 && (frame.flags_2484 & 0x00008000u) == 0) {
        Vector4 heading_axis = reckoning.heading_axis_1200;
        if ((((frame.flags_2468 >> 20) & 1u) != 0) != state.start_flipped_210) {
            heading_axis = flip_sign_bits(heading_axis);
        }

        float unwrapped_angle = 0.0f;
        if (runtime.dot3(reckoning.landing_normal_1152, state.landing_normal_64) > DIRECTION_EPSILON) {
            unwrapped_angle =
                runtime.signed_angle_about_axis(heading_axis, state.landing_heading_80, reckoning.landing_normal_1152);
            if ((frame.flags_2468 & 0x00100000u) != 0) {
                unwrapped_angle += PI;
            }
        }

        float corrected_angle = runtime.wrap_angle_8258db98(unwrapped_angle);
        const float heading_degrees = corrected_angle * RADIANS_TO_DEGREES;
        const float max_adjust = settings.max_heading_adjust_vs_up_y_160.evaluate(frame.skater_up_544[1]);
        if (rs::abs(heading_degrees) > max_adjust) {
            const float wrapped_pi = runtime.wrap_angle_8258db98(PI);
            corrected_angle = runtime.wrap_angle_8258db98(wrapped_pi + corrected_angle);
        }

        float current_speed = reckoning.body_spin_speed_1572;
        const float remaining = state.collision_time_196 - state.time_in_state_180;
        const float alignment_time = select(remaining - ALIGNMENT_DELAY, remaining, ALIGNMENT_DELAY);
        const float reciprocal_time = 1.0f / alignment_time;
        float target = (reciprocal_time * corrected_angle) * AUTO_SPIN_SCALE;

        if (rs::abs(current_speed) < DIRECTION_EPSILON) {
            current_speed = 0.0f;
        }
        if (current_speed * target < 0.0f) {
            const float alternate = runtime.wrap_angle_8258db98(unwrapped_angle + PI);
            if (FORTY_FIVE_DEGREES * DEGREES_TO_RADIANS > rs::abs(alternate) &&
                (reciprocal_time * alternate) * current_speed > 0.0f) {
                target = reciprocal_time * alternate;
            }
        }

        result = native_clamp(target, -mode.body_spin_speed_limit_56, mode.body_spin_speed_limit_56);
        if (rs::abs(result) < settings.min_auto_body_speed_424) {
            result *= HALF;
        }
        if (rs::abs(unwrapped_angle) < settings.min_auto_body_speed_424) {
            result = (unwrapped_angle / frame.delta_time_2604) * HALF;
        }
        if (result * current_speed < 0.0f) {
            result *= REVERSE_SCALE;
        } else if (rs::abs(current_speed) > rs::abs(result)) {
            result *= DECELERATION_SCALE;
        }
    }

    return result;
}

}  // namespace s3::air::known::body
