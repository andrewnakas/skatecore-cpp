// Port of skate-core/src/riding/push_animation.rs.
#include "s3/core/riding/push_animation.hpp"

namespace s3::riding::push_animation {

namespace {

float unit_interval(float value) {
    // Native fsel sequence also canonicalizes negative zero to positive zero.
    const float lower = -value >= 0.0f ? 0.0f : value;
    return 1.0f - lower >= 0.0f ? lower : 1.0f;
}

float approach(float current, float target, float rate, float dt) {
    const float difference = target - current;
    float result;
    if (rs::abs(difference) > 0.001f) {
        if (difference > 0.0f) {
            const float candidate = rs::mul_add(dt, rate, current);
            result = candidate - target >= 0.0f ? target : candidate;
        } else {
            // Native fnmsubs, followed by fsel; preserve the fused operation.
            const float candidate = -rs::mul_add(dt, rate, -current);
            result = candidate - target >= 0.0f ? candidate : target;
        }
    } else {
        result = target;
    }
    return unit_interval(result);
}

}  // namespace

float PushAnimationCurves::strength(float held_seconds, float speed) const {
    const float duration = button_time_max.evaluate(speed);
    return button_time_to_dv.evaluate(unit_interval(held_seconds / duration));
}

PushBlendParameters PushAnimationCurves::update_blend(PushBlendParameters current, PushBlendParameters target,
                                                      float forward_speed, float delta_seconds) const {
    if (current.vel_e < 0.0f) current = target;
    // This factor is a native constant at 0x820849C8, not the graph dt.
    const float frame_seconds = rs::from_bits(0x3c888889u);
    const float velocity_rate = 1.0f / (blend_speed_over_frames.evaluate(current.vel_e) * frame_seconds);
    const float speed = forward_speed >= 0.0f ? forward_speed : 0.0f;
    const float strength_rate = 1.0f / (blend_acc_over_frames.evaluate(speed) * frame_seconds);
    PushBlendParameters out;
    out.hstr_vel_b = approach(current.hstr_vel_b, target.hstr_vel_b, velocity_rate, delta_seconds);
    out.lstr_vel_b = approach(current.lstr_vel_b, target.lstr_vel_b, velocity_rate, delta_seconds);
    out.vel_e = approach(current.vel_e, target.vel_e, strength_rate, delta_seconds);
    return out;
}

}  // namespace s3::riding::push_animation
