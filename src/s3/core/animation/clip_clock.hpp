// Port of skate-core/src/animation/clip_clock.rs.
//! Andale Clip timing: 0x82D25A00, 0x825902C0, 0x827B8BD8,
//! 0x827B8C38, 0x827B8B90; attribute status 0x82D264C0.
#pragma once
#include <cstdint>

#include "s3/core/rs.hpp"

namespace s3::animation::clip_clock {

struct AdvanceResult {
    /// Native output byte +1. Exact equality with the end does not cross it.
    bool crossed_end = false;
    /// Native output +4; unchanged on updates that do not cross the end.
    float overshoot = 0.0f;
    /// Native output +8, measured before wrapping, so it may be negative.
    float remaining_before_wrap = 0.0f;
};
S3_DERIVE_EQ(AdvanceResult, crossed_end, overshoot, remaining_before_wrap)

struct ClipClock {
    float frames = 0.0f;
    float fps = 0.0f;
    float base_speed = 0.0f;
    float speed = 0.0f;
    float length = 0.0f;
    float time = 0.0f;
    float previous_time = 0.0f;
    /// Native +40; retained until pose evaluation commits previous time.
    std::uint32_t loops_since_evaluation = 0;
    bool looping = false;
    /// Clip flags word bit 30 / native object byte +49.
    bool phase_controlled = false;

    void set_time(float time_) {
        time = time_;
        previous_time = time_;
        loops_since_evaluation = 0;
    }

    /// Preserve the native multiply/divide order when changing playback rate.
    /// In particular, simplifying this to old_speed/new_speed changes rounding.
    void set_speed(float speed_);

    float sample_time() const { return length - time >= 0.0f ? time : length; }

    void advance(float delta_seconds, float phase, AdvanceResult& result);

    /// 0x82D25C20/24, called when pose evaluation's update-history flag is set.
    /// Advancing time alone does not perform this commit in the elapsed mode.
    void commit_evaluation() {
        previous_time = time;
        loops_since_evaluation = 0;
    }

    /// Return the native status byte used by GetAttribute/GetAttributes and
    /// the physics contact consumer. Times in the bank are normalized to length.
    std::uint8_t attribute_status(float begin, float end) const;
};

}  // namespace s3::animation::clip_clock
