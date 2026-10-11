// Port of skate-core/src/riding/pumping/controller.rs.
// Recovered scalar/control flow of TU3 82D8F228 and 82D8F470.
//
// Production uses geometry::NativePumpingGeometry for the original vector
// operations, refinement order and inverse-cosine kernel. The Ground adapter
// supplies the current physical sample and the selected stock mode each tick.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/riding/pumping/settings.hpp"
#include "s3/core/riding/pumping/state.hpp"

namespace s3::riding::pumping::controller {

using settings::PumpingMode;
using settings::PumpingSettings;
using state::PumpingState;

/// Literal loaded by Ground Update 82D37EA0, independent of Processed+2604.
inline constexpr float GROUND_PUMPING_TIMESTEP = rs::from_bits(0x3c888889u);

struct PumpingSample {
    /// Ground caller: ProcessedPhysIn+112, ground context+1216, skeleton+11008.
    std::array<float, 4> position{};
    std::array<float, 4> normal{};
    std::array<float, 4> com_to_deck_world{};
    /// Ground's signed-angle difference calculation supplies the absolute angle.
    float deck_angle = 0.0f;
    /// Ground caller extracts ProcessedPhysIn+2476 bit 1.
    std::uint8_t intentional_pumping = 0;
};

/// Rust trait `PumpingGeometry` (duck-typed template parameter `G`).
/// Recovered numeric operations, implemented by geometry::NativePumpingGeometry.
/// Keeping this boundary explicit allows independent measurements to validate
/// the surrounding lifecycle without asserting numeric parity from mocks.
///   using Error = ...;
///   /// 82D8F280..290: three-component COM-to-deck projection onto the normal.
///   rs::Result<float, Error> height(std::array<float,4> normal, std::array<float,4> com_to_deck);
///   /// 82D8F4A0..574: displacement divided by dt, then its length.
///   rs::Result<float, Error> speed(std::array<float,4> previous, std::array<float,4> current, float dt);
///   /// 82D8F590..6AC: dot(cross(previous_normal, normal)/dt,
///   /// safe_normalize(cross(position-previous_position, normal))).
///   /// Preserve the recovered fused cross-product and refinement order.
///   rs::Result<float, Error> angular_speed(std::array<float,4> previous_position,
///       std::array<float,4> previous_normal, const PumpingSample& sample, float dt);
///   /// 82453298 called at 82D8F6EC, with previous normal.y clamped to [0,1].
///   rs::Result<float, Error> inclination_radians(float clamped_normal_y);

namespace detail {
inline float unit(float value) {
    const float lower = -value >= 0.0f ? 0.0f : value;
    return 1.0f - lower >= 0.0f ? lower : 1.0f;
}
}  // namespace detail

#define S3_PUMP_TRY(out, expr)                                \
    float out;                                                \
    {                                                         \
        auto s3_r_ = (expr);                                  \
        if (s3_r_.is_err()) return rs::make_err(s3_r_.unwrap_err()); \
        out = s3_r_.unwrap();                                 \
    }

/// Scalar portion of Calculate, including output write/curve evaluation order.
/// Does not update the Update-owned cached pose, pump time or pump acceleration.
template <class G>
rs::Result<float, typename G::Error> calculate(PumpingState& state, const PumpingSettings& settings,
                                               const PumpingSample& sample, float timestep, G& geometry) {
    S3_PUMP_TRY(speed, geometry.speed(state.previous_position, sample.position, timestep))
    const float speed_effect = settings.pump_vs_speed.evaluate(speed);
    S3_PUMP_TRY(angular_speed,
                geometry.angular_speed(state.previous_position, state.previous_normal, sample, timestep))
    const float measured = angular_speed * settings.angular_speed_damping;
    state.angular_speed = rs::mul_add(1.0f - settings.angular_speed_damping, state.angular_speed, measured);
    state.absorption = -(speed * state.angular_speed);
    S3_PUMP_TRY(inclination, geometry.inclination_radians(detail::unit(state.previous_normal[1])))
    // Stock 822F8B7C: 2/pi rounded to binary32. This is not host acos().
    const float radians_to_quarter_turns = rs::from_bits(0x3f22f983u);
    const float ground_angle = detail::unit(radians_to_quarter_turns * inclination);
    state.ground_normal_absorption =
        settings.compression_vs_ground_angle.evaluate(ground_angle) * settings.ground_compression_scale;
    state.minimum_crouch = settings.min_crouch_vs_ground_angle.evaluate(ground_angle);
    state.deck_angle_absorption =
        settings.compression_vs_deck_angle.evaluate(sample.deck_angle * radians_to_quarter_turns);
    const float deck_compression = settings.deck_compression_scale * state.deck_angle_absorption;
    const float pumping = state.angular_speed * speed_effect;
    if (deck_compression * deck_compression > state.ground_normal_absorption * state.ground_normal_absorption) {
        state.ground_normal_absorption = deck_compression;
    }
    return pumping;
}

/// Executes the recovered lifecycle only when the required geometry succeeds.
/// An error leaves the already executed prefix visible, and must abort the
/// caller's simulation tick; it is never permission to publish partial forces.
template <class G>
rs::Result<rs::Unit, typename G::Error> update(PumpingState& state, const PumpingSettings& settings, PumpingMode mode,
                                               PumpingSample sample, float timestep, G& geometry) {
    state.pump_acceleration = 0.0f;
    S3_PUMP_TRY(height, geometry.height(sample.normal, sample.com_to_deck_world))
    if (state.record_valid) {
        state.intentional_pumping = sample.intentional_pumping;
        const float change = height - state.previous_height;
        const float rising_change = change >= 0.0f ? change : 0.0f;
        const float retained = (1.0f - settings.height_change_damping) * state.smoothed_height_change;
        const float smoothed = rs::mul_add(rising_change, settings.height_change_damping, retained);
        state.smoothed_height_change = smoothed;
        // bge after fcmpu takes the unordered path too: do not replace with >=.
        float effect = smoothed < settings.minimum_height_change ? 0.0f : smoothed;
        state.pumping_time = effect > 0.0f ? state.pumping_time + timestep : 0.0f;
        if (rs::abs(effect) > settings.maximum_height_change) {
            const float sign = effect >= 0.0f ? (effect > 0.0f ? 1.0f : 0.0f) : -1.0f;
            effect = sign * settings.maximum_height_change;
        }
        effect = settings.pump_vs_time.evaluate(state.pumping_time) * effect;
        S3_PUMP_TRY(pumping, calculate(state, settings, sample, timestep, geometry))
        state.pumping = pumping;
        const float factor = pumping * effect > 0.0f ? mode.acceleration_factor : mode.absorption_factor;
        const float raw = (pumping * factor) * effect;
        const float lower = -(mode.maximum_absorption_per_second * timestep);
        const float upper = mode.maximum_acceleration_per_second * timestep;
        const float lower_clamped = lower - raw >= 0.0f ? lower : raw;
        state.pump_acceleration = upper - lower_clamped >= 0.0f ? lower_clamped : upper;
    }
    state.previous_position = sample.position;
    state.previous_normal = sample.normal;
    state.record_valid = true;
    state.previous_height = height;
    return rs::Unit{};
}

#undef S3_PUMP_TRY

/// Pumping slice of Ground Update 82D37EA8. Force assembly must follow only
/// after this succeeds; it has its own ProcessedPhysIn timestep for force scaling.
template <class G>
rs::Result<rs::Unit, typename G::Error> update_ground(PumpingState& state, const PumpingSettings& settings,
                                                      PumpingMode mode, PumpingSample sample, G& geometry) {
    return update(state, settings, mode, sample, GROUND_PUMPING_TIMESTEP, geometry);
}

}  // namespace s3::riding::pumping::controller
