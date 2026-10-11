// Port of skate-core/src/animation/channel_playback.rs.
//! Persistent channel timing from TU3 NewChannel82D1CC20, AddTime82D266A8,
//! RemoveExpiredChannels82D1DA00 and EndChannelPrematurely82D1D6A8.
#pragma once
#include <cstdint>
#include <vector>

#include "s3/core/animation/clip_clock.hpp"
#include "s3/core/animation/output/attributes.hpp"
#include "s3/core/animation/slice.hpp"
#include "s3/core/rs.hpp"

namespace s3::animation::channel_playback {

using clip_clock::AdvanceResult;
using output::attributes::AnimationAttribute;

struct ChannelSettings {
    std::int32_t priority = 0;
    bool keep_alive = false;
    bool mirrored = false;
    float speed = 0.0f;
    float blend_in = 0.0f;
    bool hold_during_blend_in = false;
    float blend_out = 0.0f;
    bool hold_during_blend_out = false;
    bool use_attributes = false;
};
S3_DERIVE_EQ(ChannelSettings, priority, keep_alive, mirrored, speed, blend_in, hold_during_blend_in, blend_out,
             hold_during_blend_out, use_attributes)

struct ChannelPlayback {
    ChannelSettings settings;
    float weight = 0.0f;
    float influence = 0.0f;
    float blend_in_elapsed = 0.0f;
    float blend_out_elapsed = 0.0f;
    float resurrection_elapsed = 0.0f;
    float resurrection_duration = 0.0f;
    std::uint32_t flags = 0;

    static ChannelPlayback new_(ChannelSettings settings);
    ///Called before advancing any trees, in the native actor AddTime order.
    bool expired() const;
    void end();
    void end_with(float seconds, bool from_last_frame);
    bool can_transition(bool resurrect) const;
    ///StartTransition82D1D398..D47C preserves fade-in time/influence and
    ///priority, replaces fade-out settings, and optionally restores a fading
    ///channel from its existing coefficient rather than restarting at zero.
    void transition(ChannelSettings settings, bool resurrect);
    ///Returns whether the channel child should advance. The base has already
    ///advanced; fade weights use PRE-update elapsed times, then increment them.
    bool advance(float dt, float length, float time);
    void did_advance(AdvanceResult property);
    std::vector<AnimationAttribute> merge_attributes(std::vector<AnimationAttribute> base,
                                                     Slice<const AnimationAttribute> channel) const;
};

}  // namespace s3::animation::channel_playback
