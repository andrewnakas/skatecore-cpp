// Port of skate-core/src/animation/crouching.rs.
#include "s3/core/animation/crouching.hpp"

namespace s3::animation::crouching {

namespace {
float minimum(float a, float b) { return a - b >= 0.0f ? b : a; }
float maximum(float a, float b) { return a - b >= 0.0f ? a : b; }
float bound(float v, float lo, float hi) { return minimum(hi, maximum(lo, v)); }
float nonnegative(float v) { return -v >= 0.0f ? 0.0f : v; }
float unit(float v) { return minimum(1.0f, nonnegative(v)); }
}  // namespace

State State::begin(Physical physical, const Settings& settings) {
    const float maximum_ = settings.maximum_height.evaluate(physical.body_164);
    const float fraction =
        1.0f - unit((physical.animation_height_72 - settings.minimum_height) / (maximum_ - settings.minimum_height));
    State s;
    s.fraction = fraction;
    s.pump_idle_time = 0.5f;
    return s;
}

Output State::update(Physical p, Intents intents, float dt, const Settings& s) {
    const float restoring = -s.absorption_upforce.evaluate(1.0f - fraction);
    float board = bound(-p.force_516 / s.maximum_force, -1.0f, 1.0f);
    absorbed_velocity += p.body_84;
    absorbed_velocity = s.skateboard_damping * absorbed_velocity;
    if (absorbed_velocity < 0.0f) {
        board = -board;
    }
    const float ground = bound(-p.ground_force_520 / s.maximum_ground_force, -1.0f, 1.0f);
    float absorption;
    if (board < 0.0f) {
        absorption = ground > 0.0f ? ground + board : board;
    } else {
        absorption = maximum(board, ground);
    }
    if (absorption < 0.0f) {
        if (!(absorption < restoring)) {
            absorption = restoring;
        }
    } else {
        absorption += restoring;
    }
    const float angle = intents.auto_pump_angle.value_or(0.0f);
    const float magnitude = intents.auto_pump_magnitude.value_or(0.0f);
    const AutoPumpSettings& a = s.auto_pump;
    const bool started = rs::abs(previous_angle) < a.intent_angle_region &&
                         previous_magnitude < a.intent_magnitude_start && rs::abs(angle) < a.intent_angle_region &&
                         magnitude > a.intent_magnitude_start;
    previous_angle = angle;
    previous_magnitude = magnitude;
    filtered_potential = rs::mul_add(p.body_188, a.potential_blend, (1.0f - a.potential_blend) * filtered_potential);
    const bool sufficient_potential = !(filtered_potential < a.potential_threshold);
    if (started) {
        auto_time = 0.0f;
        auto_state = 1;
    }
    if (auto_state == 1 && auto_time > a.crouch_time) {
        auto_state = 2;
    }
    const bool new_auto_pump =
        (auto_state == 1 || auto_state == 2) && fraction > a.sufficient_crouch && sufficient_potential;
    if (new_auto_pump) {
        auto_state = 3;
    }
    if ((auto_state == 2 || auto_state == 3) && !(fraction > a.standing_threshold + p.minimum_crouch_528)) {
        auto_state = 0;
    }
    switch (auto_state) {
        case 0: auto_fraction = 0.0f; break;
        case 1:
            auto_fraction = minimum(a.maximum_crouch.evaluate(p.body_164), a.crouch_speed + auto_fraction);
            auto_time = minimum(a.crouch_time + 1.0f, dt + auto_time);
            break;
        case 2: auto_fraction = nonnegative(auto_fraction - a.rise_speed); break;
        case 3: auto_fraction = nonnegative(auto_fraction - a.pump_speed); break;
        default: break;
    }
    float crouch = intents.crouch.value_or(0.0f);
    if (!intents.motion_flag_108) {
        crouch = maximum(crouch, intents.hard_turn_crouch.value_or(0.0f));
    }
    if (intents.motion_flag_108 && intents.manual && *intents.manual < 0.0f) {
        crouch = maximum(crouch, rs::from_bits(0x3ecccccd));
    }
    const float blend = unit(s.input_blend);
    filtered_input = rs::mul_add(maximum(crouch, auto_fraction), blend, (1.0f - blend) * filtered_input);
    if (rs::abs(filtered_input) < rs::from_bits(0x3ba3d70a)) {
        filtered_input = 0.0f;
    }
    float delta = s.absorption_factor * absorption;
    if (crouch > 0.0f || auto_fraction > 0.0f || (player_pumping && fraction > p.minimum_crouch_528)) {
        player_pumping = true;
        delta = maximum(minimum(1.0f, maximum(p.minimum_crouch_528, filtered_input)) - fraction, -s.maximum_delta);
        pump_idle_time = 0.0f;
    } else {
        player_pumping = false;
        // Native literal820849C8, deliberately not graph delta time.
        pump_idle_time += rs::from_bits(0x3c888889);
    }
    const bool player_controlled_pump = pump_idle_time < 0.5f;
    const float minimum_delta = -(s.pump_maxspeed.evaluate(1.0f - fraction) * s.pump_vertical_speed);
    const float maximum_delta = s.pump_maxspeed.evaluate(fraction) * s.pump_vertical_speed;
    delta = bound(delta, minimum_delta, maximum_delta);
    delta_fraction = bound(delta, delta_fraction - s.maximum_delta_delta, s.maximum_delta_delta + delta_fraction);
    fraction = bound(fraction + delta_fraction, p.minimum_crouch_528, 1.0f);
    const float from_deck = s.maximum_crouch_from_deck * p.deck_angle_532;
    if (from_deck > fraction) {
        fraction = from_deck;
    }
    const float height_ratio = unit(rs::mul_add(-s.maximum_ratio, fraction, 1.0f));
    const float maximum_height = s.maximum_height.evaluate(p.body_164);
    return Output{rs::mul_add(maximum_height - s.minimum_height, height_ratio, s.minimum_height), new_auto_pump,
                  player_controlled_pump};
}

}  // namespace s3::animation::crouching
