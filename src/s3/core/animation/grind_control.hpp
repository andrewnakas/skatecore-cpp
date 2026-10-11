// Port of skate-core/src/animation/grind_control.rs.
//! S3 TU3 GrindControlFade82BB0D40 and ControlGrindCrouch82BB10F8.
//! Adapted from the authorized ZIP after independent original-image comparison.
//! Animation selection, physical observations and tuning remain external inputs.
#pragma once
#include <optional>

#include "s3/core/animation/grind_control/facing.hpp"
#include "s3/core/rs.hpp"

namespace s3::animation::grind_control {

struct FadeSettings {
    /// anim_motion/grind_twist: offsets1640/1644/1648/1652.
    float response = 0.0f;
    float input_scale = 0.0f;
    float acceleration = 0.0f;
    float maximum_step = 0.0f;
};

inline float bound(float value, float minimum, float maximum) {
    value = minimum - value >= 0.0f ? minimum : value;
    return maximum - value >= 0.0f ? value : maximum;
}

struct Fade {
    bool just_began = false;
    float elapsed = 0.0f;
    float value = 0.0f;
    float target = 0.0f;
    float step = 0.0f;
    float minimum = 0.0f;
    float maximum = 0.0f;

    /// Begin82BB0A30 supplies the actual queried endpoints and mirrored twist.
    float begin(float minimum_, float maximum_, float twist) {
        const float v = bound(twist, minimum_, maximum_);
        *this = Fade{};
        just_began = true;
        minimum = minimum_;
        maximum = maximum_;
        value = v;
        target = v;
        return v;
    }
    /// First Update only clears the latch. dt affects elapsed, NOT twist speed.
    std::optional<float> update(float dt, float intent, FadeSettings settings) {
        if (just_began) {
            just_began = false;
            return std::nullopt;
        }
        elapsed += dt;
        target = bound(rs::mul_add(settings.input_scale, intent, target), minimum, maximum);
        const float desired = rs::mul_add(1.0f - settings.response, value, target * settings.response);
        const float s = bound(desired - value, step - settings.acceleration, step + settings.acceleration);
        step = bound(s, -settings.maximum_step, settings.maximum_step);
        value = bound(value + step, minimum, maximum);
        return value;
    }
};

/// S3 always includes physical crouch; S2's optional enable switch is absent.
/// anim_motion/grind_height: min1760/max1792/rate1816; previous starts at Anim+72.
inline float crouch(float previous, float intent, float grind_crouch, float minimum, float maximum, float rate,
                    float dt) {
    const float input = bound(1.0f - intent, minimum, maximum);
    const float physical = bound(1.0f - grind_crouch, minimum, maximum);
    const float desired = input - physical >= 0.0f ? physical : input;
    const float step = dt * rate;
    return bound(desired, previous - step, previous + step);
}

/// Begin82BB0CBC..CE4: mirror the Skeleton+504 angle exactly once.
inline float mirrored_twist(float twist, bool mirrored) {
    if (!mirrored) {
        return twist;
    }
    return twist >= 0.0f ? rs::f32::consts::PI - twist : -rs::f32::consts::PI - twist;
}

}  // namespace s3::animation::grind_control
