// Port of skate-core/src/air/reckoning/mod.rs.
#include "s3/core/air/reckoning.hpp"

#include "s3/core/air/body_flip.hpp"

namespace s3::air::reckoning {

using body_flip::BodyFlipInput;
using body_flip::BodyFlipState;
using math::angle;
using math::length;
using math::limit_angle;
using math::normalize;
using math::normalize_safe;
using math::rotate_heading;
using math::wrap;

void update(GroundOrientation& orientation, ReckoningFrames& frames, BodySpinState& body_spin, AirState& state,
            const Settings& settings, const Input& input) {
    const std::array<float, 4> old_up = lanes(orientation.up);
    orientation.dynamic_up = orientation.up;
    frames.target_lean_angle = 0.0f;
    state.secondary_lean_angle = 0.0f;
    const std::array<float, 4> ground_normal =
        orientation.ground_filter.update(settings.ground_normal_smoothing, input.landing_normal);
    orientation.ground_normal = vector(ground_normal);

    if (input.additive_spin) {
        state.spin_speed = input.grind_adjusted_body_spin + input.target_spin;
    } else if (input.direct_spin) {
        state.spin_speed = input.target_spin;
    } else {
        body_spin::update(body_spin, settings.body_spin, input.physical_body_spin, input.target_spin, true,
                          static_cast<std::uint8_t>(input.easy_body_spins ? 1 : 0));
        state.spin_speed = body_spin.speed();
    }
    state.spin_angle = rs::mul_add(state.spin_speed, rs::from_bits(0x3c888889u), state.spin_angle);
    if (state.flip_active) {
        // A short call-boundary view, not a second retained owner of matrix1008.
        BodyFlipState flip{
            state.flip_angle, state.flip_speed, state.flip_requested_speed, state.spin_transform, frames.body_flip,
        };
        BodyFlipInput flip_input;
        flip_input.requested_speed = input.flip_request;
        flip_input.spin_angle = state.spin_angle;
        flip_input.normal = old_up;
        flip_input.flip_axis = state.flip_axis;
        flip_input.timestep = input.timestep;
        flip_input.perfect_body_flips = input.perfect_body_flips;
        body_flip::update(flip, settings.body_flip, flip_input);
        state.flip_angle = flip.angle;
        state.flip_speed = flip.speed;
        state.flip_requested_speed = flip.requested_speed;
        state.spin_transform = flip.spin_transform;
        frames.body_flip = flip.combined_transform;
    }

    const float wrapped = wrap(angle(old_up, ground_normal));
    const float maximum = wrapped * input.normal_blend;
    std::array<float, 4> target;
    if (rs::abs(wrapped) >= rs::from_bits(0x40c8f61eu)) {
        if (rs::abs(old_up[1]) < rs::from_bits(0x3f7d70a4u)) {
            target = {0.0f, 1.0f, 0.0f, 0.0f};
        } else {
            target = {1.0f, 0.0f, 0.0f, 0.0f};
        }
    } else {
        target = ground_normal;
    }
    const std::array<float, 4> candidate = normalize(limit_angle(target, old_up, maximum));
    const float max_delta = settings.max_up_angle_delta.evaluate(length(input.com_to_deck));
    const std::array<float, 4> selected = rs::abs(wrap(angle(candidate, old_up))) > max_delta
                                              ? limit_angle(candidate, old_up, max_delta)
                                              : candidate;
    const std::array<float, 4> up = normalize_safe(selected, old_up);
    orientation.up_velocity = vector({up[0] - old_up[0], up[1] - old_up[1], up[2] - old_up[2], up[3] - old_up[3]});
    orientation.up = vector(up);
    orientation.target = orientation.up;

    if (!state.flip_active) {
        frames.heading = rotate_heading(frames.heading, up, input.timestep * state.spin_speed);
    }
    // 82D8C868 advances histories using retained control words, then publishes
    // the chosen up into each current-position slot; no filter reinitialization.
    orientation.slow_filter.filter(up);
    orientation.fast_filter.filter(up);
    orientation.slow_filter.publish_current(up);
    orientation.fast_filter.publish_current(up);
    frames.calculate_transform(up, ground_normal);
    frames.calculate_tilt(input.reverse_stance, settings.tilt_vs_rotation, settings.tilt_vs_slope);
}

void update_plant(GroundOrientation& orientation, ReckoningFrames& frames, BodySpinState& body_spin, AirState& state,
                  const Settings& settings, std::array<float, 4> up_in, std::array<float, 4> heading, bool reverse) {
    frames.heading = heading;
    body_spin::update(body_spin, settings.body_spin, 0.0f, 0.0f, true, 0);
    const std::array<float, 4> previous = lanes(orientation.up);
    orientation.dynamic_up = orientation.up;
    frames.target_lean_angle = 0.0f;
    state.secondary_lean_angle = 0.0f;
    const std::array<float, 4> normal =
        normalize(orientation.ground_filter.update(settings.ground_normal_smoothing, up_in));
    orientation.ground_normal = vector(normal);
    const std::array<float, 4> up = normalize_safe(limit_angle(normal, previous, 1.0f), previous);
    orientation.up_velocity =
        vector({up[0] - previous[0], up[1] - previous[1], up[2] - previous[2], up[3] - previous[3]});
    orientation.up = vector(up);
    orientation.target = orientation.up;
    orientation.slow_filter.filter(up);
    orientation.fast_filter.filter(up);
    orientation.slow_filter.publish_current(up);
    orientation.fast_filter.publish_current(up);
    frames.calculate_transform(up, normal);
    frames.calculate_tilt(reverse, settings.tilt_vs_rotation, settings.tilt_vs_slope);
}

}  // namespace s3::air::reckoning
