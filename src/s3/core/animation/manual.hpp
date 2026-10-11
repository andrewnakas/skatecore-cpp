// Port of skate-core/src/animation/manual.rs.
//! SetManualAngle Begin82BA8C48/Update82BA8C60. Animation selection only;
//! physical balance is separately emitted by the authored AttachIntent node.
#pragma once
#include <optional>

#include "s3/core/point_graph.hpp"
#include "s3/core/rs.hpp"

namespace s3::animation::manual {

using point_graph::PointGraph;

struct Settings {
    /// anim_motion/manual layout1216..1276, native eight-point evaluator.
    PointGraph<8> balance;
    /// Layout1800/1804. Limits are per graph update, not per second.
    float velocity_limit = 0.0f;
    float acceleration_limit = 0.0f;
};
S3_DERIVE_EQ(Settings, balance, velocity_limit, acceleration_limit)

inline float clamp(float value, float limit) {
    // Preserve the source fsubs/fsel selection, including signed zero.
    const float lower = -limit - value >= -0.0f ? -limit : value;
    return limit - lower >= -0.0f ? lower : limit;
}

struct State {
    float angle = 0.0f;
    float velocity = 0.0f;

    void begin() { *this = State{}; }

    float update(std::optional<float> manual, const Settings& settings) {
        // The native lookup begins with zero; an absent intent leaves it zero.
        const float m = manual.value_or(0.0f);
        float target = settings.balance.evaluate(rs::abs(m));
        target = m >= -0.0f ? target : -target;
        const float desired_velocity = clamp(target - angle, settings.velocity_limit);
        const float acceleration = clamp(desired_velocity - velocity, settings.acceleration_limit);
        velocity += acceleration;
        angle += velocity;
        return angle;
    }
};
S3_DERIVE_EQ(State, angle, velocity)

}  // namespace s3::animation::manual
