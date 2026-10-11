// Port of skate-core/src/physics/manual/controller.rs.
// CalcManualEffect 82C047F0 control/scalar response and output construction.
// The native angle-between-vectors measurement remains a required dependency.
#pragma once
#include <array>
#include <utility>
#include <variant>

#include "s3/core/physics/manual/angle.hpp"
#include "s3/core/physics/manual/settings.hpp"
#include "s3/core/physics/manual/state.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::physics::manual::controller {

using settings::ManualMode;
using settings::ManualSettings;
using state::ManualState;

struct ManualInput {
    /// ProcessedPhysIn+2720 and +2620.
    float balance = 0.0f;
    float flipped_controls = 0.0f;
    /// +2664, +2616 and +2768. Noise time is independent of manual elapsed.
    float procedural_noise_time = 0.0f;
    float absolute_speed = 0.0f;
    float animation_noise = 0.0f;
    /// +2604; this controller does not replace it with a fixed 1/60.
    float timestep = 0.0f;
    /// The explicit CalcManualEffect argument; Ground callers pass false.
    bool powersliding = false;
    /// Processed2468 bit29 selects BrakeTiltAngle and correction-force gates.
    bool braking = false;
    /// Processed2472 bits27 and26, respectively. Physical axle identities are
    /// not assumed here; the signed balance gates are preserved directly.
    bool positive_balance_contact = false;
    bool negative_balance_contact = false;
    /// Processed2468 bit20 reverses the point-selection balance sign.
    bool reversed_point_selection = false;
    /// Reference frame from board+292, matrix+752: first and third columns.
    std::array<float, 4> reference_x{};
    std::array<float, 4> reference_z{};
    /// Deck part6 transform's z-axis from 82585CB0, not an animation pose.
    std::array<float, 4> deck_z{};
    /// Processed effective transform third column+224 and ground velocity+416.
    std::array<float, 4> velocity_frame_z{};
    /// Legacy field name: this is Processed416, NOT angular velocity720.
    /// Native82C04C88 projects it into the effective frame;82C04E60..EC4
    /// emits -400 times this same translational velocity for correction.
    std::array<float, 4> angular_velocity_world{};
    /// Front/back truck drive-frame translations (7840+48 and7904+48).
    ///82C0B9C0 steering rotates the bases and preserves these translations.
    std::array<float, 4> correction_point_7888{};
    std::array<float, 4> correction_point_7952{};
};

/// Exact 8296EC98 dependency: degeneracy gates, normalization, acos and signed
/// cross-product selection. There is deliberately no host atan2/acos fallback.
///
/// Rust trait `ManualAngleMeasurement`: a C++ implementor provides
///   using Error = ...;
///   rs::Result<float, Error> angle_between(std::array<float,4> deck_z,
///       std::array<float,4> reference_z, std::array<float,4> reference_x);

/// Rust `enum ManualError<E> { Angle(AngleError), Measurement(E) }`.
template <class E>
struct ManualError {
    std::variant<angle::AngleError, E> value;

    static ManualError Angle(angle::AngleError e) {
        return ManualError{std::variant<angle::AngleError, E>(std::in_place_index<0>, e)};
    }
    static ManualError Measurement(E e) {
        return ManualError{std::variant<angle::AngleError, E>(std::in_place_index<1>, std::move(e))};
    }
    bool operator==(const ManualError& o) const { return value == o.value; }
    bool operator!=(const ManualError& o) const { return !(value == o.value); }
};

struct ManualEffect {
    /// Native result+0, applied with ApplyAngularDisplacementToDeck.
    std::array<float, 4> angular_displacement{};
    /// Result+16/+32 are explicitly zeroed by this function.
    std::array<float, 4> force_world{};
    std::array<float, 4> force_point_body{};
    /// Result+48/+64, plus result+80/+81 status bytes.
    std::array<float, 4> corrective_force_world{};
    std::array<float, 4> corrective_point_body{};
    bool correction_active = false;
    bool opposing_motion_without_correction = false;
};
S3_DERIVE_EQ(ManualEffect, angular_displacement, force_world, force_point_body,
             corrective_force_world, corrective_point_body, correction_active,
             opposing_motion_without_correction)

namespace detail {
void apply_correction(ManualEffect& effect, const ManualInput& input, ManualMode mode, float target,
                      float measured);
float clamp_symmetric(float value, float bound);
}  // namespace detail

/// An unavailable measurement/conversion aborts the tick. Already executed
/// scalar-state writes remain visible, but no effect is returned for publication.
template <class G>
rs::Result<ManualEffect, ManualError<typename G::Error>> calculate(ManualState& state,
                                                                  const ManualSettings& settings,
                                                                  ManualMode mode,
                                                                  const ManualInput& input,
                                                                  G& geometry) {
    using Err = ManualError<typename G::Error>;
    ManualEffect effect{};
    float output_scale = 1.0f;
    if (input.balance == 0.0f) {
        state.reset();
    } else {
        // `?` on angle::normalize: early-return Err(ManualError::Angle(..)).
#define S3_MANUAL_NORM(out, expr)                                \
    float out;                                                   \
    {                                                            \
        const auto s3_r_ = angle::normalize(expr);               \
        if (s3_r_.is_err()) return rs::make_err(Err::Angle(s3_r_.unwrap_err())); \
        out = s3_r_.unwrap();                                    \
    }
        const auto gains = input.powersliding ? settings.powerslide : settings.manual;
        const float scaled_balance = input.balance * input.flipped_controls;
        const float sign = scaled_balance > 0.0f ? 1.0f : -1.0f;
        const float signed_target = rs::mul_add(rs::abs(scaled_balance), 0.75f, 0.25f) * sign;
        const float started = state.elapsed == 0.0f ? 0.0f : 1.0f;
        const float degrees_to_radians = rs::from_bits(0x3c8efa35u);
        S3_MANUAL_NORM(base_angle,
                       (settings.maximum_tilt_degrees * signed_target) * degrees_to_radians)
        float target;
        if (input.braking) {
            S3_MANUAL_NORM(t, (settings.brake_tilt_degrees * signed_target) * degrees_to_radians)
            target = t;
        } else {
            const float phase = (input.procedural_noise_time * settings.procedural_noise_frequency) *
                                rs::from_bits(0x40c90fdbu);
            const float noise = trigonometry::sin(phase) * settings.procedural_noise_scale;
            const float speed_scale = settings.noise_vs_speed.evaluate(input.absolute_speed);
            const float animated = input.animation_noise * settings.animation_noise_scale;
            const float disturbance = rs::mul_add(animated, input.flipped_controls, noise);
            S3_MANUAL_NORM(noise_angle, speed_scale * disturbance)
            S3_MANUAL_NORM(t, noise_angle + base_angle)
            target = t;
        }
        if (state.elapsed == 0.0f) {
            S3_MANUAL_NORM(c, settings.start_torque_scale * target)
            state.angular_correction = c;
        }
        state.target_angle = target;
        const auto measurement = geometry.angle_between(input.deck_z, input.reference_z, input.reference_x);
        if (measurement.is_err()) {
            return rs::make_err(Err::Measurement(measurement.unwrap_err()));
        }
        S3_MANUAL_NORM(measured, measurement.unwrap())
        const float angle_change = measured - state.measured_angle;
        state.measured_angle = measured;
        S3_MANUAL_NORM(raw_error, target - measured)
        const float error = detail::clamp_symmetric(raw_error, settings.maximum_angle_error);
        const float weighted_error = error * rs::from_bits(0x3d4ccccdu);
        state.filtered_angle_error =
            rs::mul_add(state.filtered_angle_error, rs::from_bits(0x3f733333u), weighted_error);
        const float derivative = detail::clamp_symmetric((angle_change * started) * gains.derivative,
                                                         settings.derivative_limit);
        const float wheel_scale =
            input.positive_balance_contact != input.negative_balance_contact ? 0.5f : 1.0f;
        const bool wrong_contact = (!input.positive_balance_contact && input.balance > 0.0f) ||
                                   (!input.negative_balance_contact && input.balance < 0.0f);
        if (wrong_contact) {
            state.angular_correction =
                (settings.torque_bleed_without_contact * state.angular_correction) * wheel_scale;
            output_scale = settings.torque_scale_without_contact * wheel_scale;
        } else {
            state.angular_correction =
                ((state.angular_correction + derivative) + state.filtered_angle_error * gains.integral) +
                error * gains.proportional;
        }
        detail::apply_correction(effect, input, mode, target, measured);
        state.elapsed = input.timestep + state.elapsed;
#undef S3_MANUAL_NORM
    }
    const float correction = state.angular_correction * output_scale;
    effect.angular_displacement =
        rs::map(input.reference_x, [&](float component) { return (-component) * correction; });
    return effect;
}

}  // namespace s3::physics::manual::controller
