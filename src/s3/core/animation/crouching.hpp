// Port of skate-core/src/animation/crouching.rs.
//! Stock riding crouch/absorption and auto-pump, TU3 82BB7DA0/82BB8818.
#pragma once
#include <cstdint>
#include <optional>

#include "s3/core/point_graph.hpp"
#include "s3/core/rs.hpp"

namespace s3::animation::crouching {

using point_graph::PointGraph;

struct AutoPumpSettings {
    PointGraph<4> maximum_crouch;
    float sufficient_crouch = 0.0f;
    float standing_threshold = 0.0f;
    float rise_speed = 0.0f;
    float pump_speed = 0.0f;
    float potential_threshold = 0.0f;
    float potential_blend = 0.0f;
    float intent_magnitude_start = 0.0f;
    float intent_angle_region = 0.0f;
    float crouch_time = 0.0f;
    float crouch_speed = 0.0f;
};

struct Settings {
    PointGraph<8> maximum_height;
    PointGraph<8> absorption_upforce;
    PointGraph<8> pump_maxspeed;
    float minimum_height = 0.0f;
    float maximum_ratio = 0.0f;
    float maximum_delta_delta = 0.0f;
    float maximum_delta = 0.0f;
    float input_blend = 0.0f;
    float pump_vertical_speed = 0.0f;
    float maximum_crouch_from_deck = 0.0f;
    float skateboard_damping = 0.0f;
    float maximum_force = 0.0f;
    float maximum_ground_force = 0.0f;
    float absorption_factor = 0.0f;
    AutoPumpSettings auto_pump;
};

/// Resolved physical publication fields. Offsets stay explicit until the
/// producer's domain identity is verified; callers must publish actual values.
struct Physical {
    float body_84 = 0.0f;
    float body_164 = 0.0f;
    float body_188 = 0.0f;
    float force_516 = 0.0f;
    float ground_force_520 = 0.0f;
    float minimum_crouch_528 = 0.0f;
    float deck_angle_532 = 0.0f;
    float animation_height_72 = 0.0f;
};

struct Intents {
    std::optional<float> auto_pump_angle;
    std::optional<float> auto_pump_magnitude;
    std::optional<float> crouch;
    std::optional<float> hard_turn_crouch;
    std::optional<float> manual;
    /// SkaterMotionGraph interface+108, used by both manual-crouch branches.
    bool motion_flag_108 = false;
};

struct Output {
    float height = 0.0f;
    bool new_auto_pump = false;
    bool player_controlled_pump = false;
};

struct State {
    float absorbed_velocity = 0.0f;
    float filtered_input = 0.0f;
    float fraction = 0.0f;
    float delta_fraction = 0.0f;
    bool player_pumping = false;
    float auto_fraction = 0.0f;
    std::uint32_t auto_state = 0;
    float auto_time = 0.0f;
    float previous_angle = 0.0f;
    float previous_magnitude = 0.0f;
    float filtered_potential = 0.0f;
    float pump_idle_time = 0.0f;

    static State begin(Physical physical, const Settings& settings);
    Output update(Physical p, Intents intents, float dt, const Settings& s);
};

}  // namespace s3::animation::crouching
