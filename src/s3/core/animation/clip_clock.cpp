// Port of skate-core/src/animation/clip_clock.rs.
#include "s3/core/animation/clip_clock.hpp"

namespace s3::animation::clip_clock {

void ClipClock::set_speed(float speed_) {
    const float new_rate = base_speed * speed_;
    const float old_rate = speed * base_speed;
    const float frames_per_second = fps * base_speed;
    const float inverse_new_rate = 1.0f / new_rate;
    time = (time * old_rate) * inverse_new_rate;
    previous_time = (previous_time * old_rate) * inverse_new_rate;
    length = (frames - 1.0f) / (frames_per_second * speed_);
    speed = speed_;
}

void ClipClock::advance(float delta_seconds, float phase, AdvanceResult& result) {
    if (phase_controlled) {
        previous_time = time;
        const float lower = -phase >= 0.0f ? 0.0f : phase;
        const float p = 1.0f - lower >= 0.0f ? lower : 1.0f;
        time = length * p;
        loops_since_evaluation = std::uint32_t(time < previous_time);
        // The native phase branch does not touch the AdvanceResult fields.
        return;
    }
    time += delta_seconds;
    result.remaining_before_wrap = length - time;
    if (!(time > length)) {
        result.crossed_end = false;
    } else if (looping) {
        // Invalid lengths cannot make progress in the native subtraction
        // loop. Fail explicitly rather than hang or invent a replacement.
        if (!(length > 0.0f && rs::is_finite(time))) S3_PANIC("invalid looping clip clock");
        while (time > length) {
            result.crossed_end = true;
            result.overshoot = time - length;
            const float next = time - length;
            if (!(next < time)) S3_PANIC("clip clock cannot advance at this precision");
            time = next;
            loops_since_evaluation = loops_since_evaluation + 1u;
        }
    } else {
        result.crossed_end = true;
        result.overshoot = time - length;
    }
}

std::uint8_t ClipClock::attribute_status(float begin, float end) const {
    if (begin == -1.0f) {
        return 6;
    }
    begin = length * begin;
    end = length * end;
    const bool intersects = loops_since_evaluation == 0 ? (end > previous_time && begin <= time)
                                                        : (end > previous_time || begin <= time);
    if (!intersects) {
        return 17;
    } else if (time < begin || time > end) {
        return 9;
    }
    return 5;
}

}  // namespace s3::animation::clip_clock
