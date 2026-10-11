// Port of skate-core/src/riding/push_behaviors.rs.
#include "s3/core/riding/push_behaviors.hpp"

#include "s3/core/physics/native_arithmetic.hpp"

namespace s3::riding::push_behaviors {

namespace {

float native_min(float a, float b) { return a - b >= 0.0f ? b : a; }
float native_max(float a, float b) { return a - b >= 0.0f ? a : b; }
float bounded(float value, float minimum, float maximum) {
    return native_min(maximum, native_max(minimum, value));
}
float nonnegative(float value) { return value >= 0.0f ? value : 0.0f; }
float unit(float value) {
    const float lower = -value >= 0.0f ? 0.0f : value;
    return 1.0f - lower >= 0.0f ? lower : 1.0f;
}

float velocity_blend(float speed, float low_length, float low_velocity, float high_length, float high_velocity) {
    const float low_distance = low_length * low_velocity;
    const float speed_distance = speed * low_length;
    const float sum = rs::mul_add(high_length, high_velocity, speed_distance);
    const float numerator = speed_distance - low_distance;
    const float denominator = -rs::mul_add(speed, high_length, -(sum - low_distance));
    return unit(numerator / denominator);
}

float blended_end(float coefficient, PushClipMetrics low, PushClipMetrics high) {
    coefficient = unit(coefficient);
    const float high_weight = coefficient * high.length;
    const float low_weight = (1.0f - coefficient) * low.length;
    return rs::mul_add(high.end_velocity, high_weight, low.end_velocity * low_weight) / (high_weight + low_weight);
}

}  // namespace

PushAttributes PushAttributes::from_clips(std::array<PushClipMetrics, 4> clips) {
    const auto begin = rs::map(clips, [](const PushClipMetrics& clip) { return clip.begin_velocity; });
    const auto delta =
        rs::map(clips, [](const PushClipMetrics& clip) { return clip.end_velocity - clip.begin_velocity; });
    PushAttributes out;
    out.clips = clips;
    out.minimum_begin_velocity = native_min(native_min(begin[0], begin[1]), native_min(begin[2], begin[3]));
    out.maximum_begin_velocity = native_max(native_max(begin[0], begin[1]), native_max(begin[2], begin[3]));
    out.minimum_delta_velocity = native_min(native_min(delta[0], delta[1]), native_min(delta[2], delta[3]));
    out.maximum_delta_velocity = native_max(native_max(delta[0], delta[1]), native_max(delta[2], delta[3]));
    return out;
}

PushBlendParameters PushAttributes::target(float forward_speed, float strength) const {
    const float speed = bounded(nonnegative(forward_speed), minimum_begin_velocity, maximum_begin_velocity);
    strength = bounded(strength, minimum_delta_velocity, maximum_delta_velocity);
    const PushClipMetrics slow_strong = clips[0];
    const PushClipMetrics fast_strong = clips[1];
    const PushClipMetrics slow_gentle = clips[2];
    const PushClipMetrics fast_gentle = clips[3];
    const float strong_speed = bounded(speed, slow_strong.begin_velocity, fast_strong.begin_velocity);
    const float gentle_speed = bounded(speed, slow_gentle.begin_velocity, fast_gentle.begin_velocity);
    const float strong = velocity_blend(strong_speed, slow_strong.length, slow_strong.begin_velocity,
                                        fast_strong.length, fast_strong.begin_velocity);
    const float gentle = velocity_blend(gentle_speed, slow_gentle.length, slow_gentle.begin_velocity,
                                        fast_gentle.length, fast_gentle.begin_velocity);
    const float strong_length = rs::mul_add(1.0f - strong, slow_strong.length, fast_strong.length * strong);
    const float gentle_length = rs::mul_add(1.0f - gentle, slow_gentle.length, fast_gentle.length * gentle);
    const float strong_end = blended_end(strong, slow_strong, fast_strong);
    const float gentle_end = blended_end(gentle, slow_gentle, fast_gentle);
    const float target_end = bounded(strength + speed, gentle_end, strong_end);
    const float strength_blend = velocity_blend(target_end, gentle_length, gentle_end, strong_length, strong_end);
    return PushBlendParameters{unit(strong), unit(gentle), unit(strength_blend)};
}

float PushAttributes::out_factor(float forward_speed, float strength, float speed_weight, float maximum_out) const {
    const float first_speed = minimum_begin_velocity + minimum_delta_velocity;
    const float speed = bounded(nonnegative(forward_speed), first_speed, maximum_begin_velocity);
    strength = bounded(strength, minimum_delta_velocity, maximum_delta_velocity);
    const float strength_factor =
        unit((strength - minimum_delta_velocity) / (maximum_delta_velocity - minimum_delta_velocity));
    const float speed_factor = unit((speed - first_speed) / (maximum_begin_velocity - first_speed));
    const float combined = rs::mul_add(strength_factor, 1.0f - speed_weight, speed_factor * speed_weight);
    const float lower = -combined >= 0.0f ? 0.0f : combined;
    return native_min(maximum_out, lower);
}

void PushState::initialize_push() {
    continue_push = false;
    current_push_dv = 0.0f;
    current = PushBlendParameters{-1.0f, -1.0f, -1.0f};
    target = current;
}

FirstPushStrength FirstPushStrength::begin(float time_since_teleport, float teleport_window) {
    FirstPushStrength out;
    out.simulated_held_seconds = 0.0f;
    out.push_from_teleport = time_since_teleport < teleport_window;
    out.first_push = true;
    out.first_update = true;
    return out;
}

void FirstPushStrength::update(PushState& state, const PushAnimationCurves& curves, std::optional<float> pushing,
                               float forward_speed, float dt) {
    if (!pushing.has_value()) {
        first_push = false;
    }
    if (first_push || push_from_teleport) {
        float held;
        if (push_from_teleport) {
            held = simulated_held_seconds;
            simulated_held_seconds = dt + simulated_held_seconds;
        } else {
            held = pushing.value_or(0.0f);
        }
        const float strength = curves.strength(held, nonnegative(forward_speed));
        if (strength > state.current_push_dv) {
            state.current_push_dv = strength;
        }
    }
    first_update = false;
}

PushCycle PushCycle::begin(PushState& state, const PushAnimationCurves& curves, PushIntents intents) {
    const bool holding = intents.pushing.has_value() && intents.configured_foot;
    state.continue_push = holding;
    PushCycle out;
    out.phase = holding ? PushCyclePhase::HoldingFirst : PushCyclePhase::WaitingNext;
    out.next_push_dv = curves.button_time_to_dv.evaluate(0.0f);
    return out;
}

void PushCycle::update(PushState& state, const PushAnimationCurves& curves, PushIntents intents, float forward_speed,
                       float maximum_holding_acceleration) {
    const float speed = nonnegative(forward_speed);
    if (phase == PushCyclePhase::HoldingFirst) {
        if (intents.pushing.has_value()) {
            const float held = *intents.pushing;
            next_push_dv = curves.strength(held, speed);
            if (next_push_dv > maximum_holding_acceleration) {
                next_push_dv = maximum_holding_acceleration;
                state.current_push_dv = maximum_holding_acceleration;
            }
            if (state.current_push_dv < next_push_dv) {
                state.current_push_dv = next_push_dv;
            }
        } else {
            phase = PushCyclePhase::WaitingNext;
            next_push_dv = curves.button_time_to_dv.evaluate(0.0f);
            state.continue_push = false;
        }
    }
    if (phase == PushCyclePhase::WaitingNext && intents.new_push && intents.configured_foot) {
        phase = PushCyclePhase::HoldingNext;
    }
    if (phase == PushCyclePhase::HoldingNext && !intents.configured_foot) {
        phase = PushCyclePhase::NextReleased;
        state.current_push_dv = next_push_dv;
    }
    if (phase == PushCyclePhase::HoldingNext) {
        next_push_dv = curves.strength(intents.pushing.value_or(0.0f), speed);
        if (state.current_push_dv < next_push_dv) {
            state.current_push_dv = next_push_dv;
        }
        state.continue_push = true;
    }
}

std::array<float, 2> PushFootFrame::out_distance(bool right_foot_) const {
    const auto& foot = right_foot_ ? right_foot : left_foot;
    std::array<float, 4> relative{};
    for (std::size_t i = 0; i < 4; ++i) relative[i] = foot[i] - deck_position[i];
    const auto z = skateboard_flipped ? rs::map(deck_z, [](float v) { return -v; }) : deck_z;
    return {physics::native_arithmetic::dot3(relative, z), physics::native_arithmetic::dot3(relative, deck_y)};
}

}  // namespace s3::riding::push_behaviors
