// Port of skate-core/src/animation/body_tilt.rs.
//! SettingBodyTilt Update82BA88A0 and CreateInstance82BA8B40.
#pragma once
#include <cstdint>
#include <optional>

#include "s3/core/point_graph.hpp"
#include "s3/core/rs.hpp"

namespace s3::animation::body_tilt {

using point_graph::PointGraph;

struct Settings {
    PointGraph<4> body_spin_factor;
    float ground_velocity = 0.0f;
    float ground_acceleration = 0.0f;
    float air_velocity = 0.0f;
    float air_acceleration = 0.0f;
};

struct Physical {
    ///PhysOutAnimation vector+0 X, consumed with mirrored stance sign.
    float lateral_tilt = 0.0f;
    ///Expanded PhysOut bundle+8 scalar204; the curve consumes its magnitude.
    float body_spin_speed = 0.0f;
    std::uint32_t filtered_category = 0;
};

inline float bound(float value, float lo, float hi) {
    const float lower = lo - value >= 0.0f ? lo : value;
    return hi - lower >= 0.0f ? lower : hi;
}

struct State {
    float value = 0.0f;
    float velocity = 0.0f;
    bool was_enabled = false;

    void disable() { was_enabled = false; }

    std::optional<float> update(bool enabled, bool mirrored, Physical p, const Settings& s) {
        if (!was_enabled && enabled) {
            value = 0.0f;
            velocity = 0.0f;
        }
        was_enabled = enabled;
        if (!enabled) {
            return std::nullopt;
        }
        //82B97140 tests fullSkaterAnim15180 bit30. The nonmirrored branch
        //flips the sign bit of the source X component before multiplication.
        const float tilt = mirrored ? p.lateral_tilt : rs::from_bits(rs::to_bits(p.lateral_tilt) ^ 0x80000000u);
        const float target = s.body_spin_factor.evaluate(rs::abs(p.body_spin_speed)) * tilt;
        const float max_velocity = p.filtered_category == 2 ? s.air_velocity : s.ground_velocity;
        const float acceleration = p.filtered_category == 2 ? s.air_acceleration : s.ground_acceleration;
        const float desired = bound(target - value, -max_velocity, max_velocity);
        const float change = bound(desired - velocity, -acceleration, acceleration);
        velocity += change;
        value += velocity;
        return value;
    }
};

}  // namespace s3::animation::body_tilt
