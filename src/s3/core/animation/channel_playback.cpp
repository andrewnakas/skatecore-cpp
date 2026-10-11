// Port of skate-core/src/animation/channel_playback.rs.
#include "s3/core/animation/channel_playback.hpp"

namespace s3::animation::channel_playback {

namespace {
float unit(float value) {
    const float low = -value >= 0.0f ? 0.0f : value;
    return 1.0f - low >= 0.0f ? low : 1.0f;
}
std::uint32_t u(bool b) { return b ? 1u : 0u; }
}  // namespace

ChannelPlayback ChannelPlayback::new_(ChannelSettings settings) {
    const std::uint32_t flags = (u(settings.keep_alive) << 28) | (u(settings.hold_during_blend_in) << 31) |
                                (u(settings.hold_during_blend_out) << 30) | (u(settings.blend_out > 0.0f) << 27) |
                                (u(settings.use_attributes) << 25);
    ChannelPlayback c;
    c.settings = settings;
    c.weight = 0.0f;
    c.influence = 1.0f;
    c.flags = flags;
    return c;
}

bool ChannelPlayback::expired() const {
    if ((flags & 0x08000000u) != 0) {
        return blend_out_elapsed >= settings.blend_out;
    }
    return (flags & 0x10000000u) == 0 && (flags & 0x04000000u) != 0;
}

void ChannelPlayback::end() {
    if (blend_out_elapsed == 0.0f) {
        flags |= 0x28000000u;
    }
}

void ChannelPlayback::end_with(float seconds, bool from_last_frame) {
    if (blend_out_elapsed == 0.0f) {
        settings.blend_out = seconds;
        settings.hold_during_blend_out = from_last_frame;
        flags = (flags & ~0x40000000u) | (u(from_last_frame) << 30) | 0x28000000u;
    }
}

bool ChannelPlayback::can_transition(bool resurrect) const { return !(blend_out_elapsed > 0.0f) || resurrect; }

void ChannelPlayback::transition(ChannelSettings s, bool resurrect) {
    if (blend_out_elapsed > 0.0f && resurrect) {
        resurrection_duration = s.blend_in;
        resurrection_elapsed = unit(1.0f - blend_out_elapsed / settings.blend_out) * s.blend_in;
    }
    blend_out_elapsed = 0.0f;
    settings.keep_alive = s.keep_alive;
    settings.blend_out = s.blend_out;
    settings.hold_during_blend_out = s.hold_during_blend_out;
    settings.use_attributes = s.use_attributes;
    flags = (flags & ~0x7a000000u) | (u(s.keep_alive) << 28) | (u(s.hold_during_blend_out) << 30) |
            (u(s.blend_out > 0.0f) << 27) | (u(s.use_attributes) << 25);
}

bool ChannelPlayback::advance(float dt, float length, float time) {
    float fade_in = 1.0f;
    if (settings.blend_in > 0.0f && blend_in_elapsed < settings.blend_in) {
        fade_in = unit(blend_in_elapsed / settings.blend_in);
        blend_in_elapsed += dt;
    }
    float remaining = length - time;
    remaining = remaining >= 0.0f ? remaining : 0.0f;
    const bool hold_end = (flags & 0x40000000u) != 0;
    const bool ended = (flags & 0x04000000u) != 0;
    const bool fade_automatically = hold_end ? ended : remaining < settings.blend_out;
    float fade_out = 1.0f;
    if (settings.blend_out > 0.0f &&
        ((flags & 0x20000000u) != 0 || ((flags & 0x10000000u) == 0 && fade_automatically))) {
        fade_out = unit(1.0f - blend_out_elapsed / settings.blend_out);
        blend_out_elapsed += dt;
    }
    float resurrection = 1.0f;
    if (resurrection_duration > 0.0f && resurrection_elapsed < resurrection_duration) {
        resurrection = unit(resurrection_elapsed / resurrection_duration);
        resurrection_elapsed += dt;
    }
    float fade = fade_in - fade_out >= 0.0f ? fade_out : fade_in;
    fade = fade - resurrection >= 0.0f ? resurrection : fade;
    weight = unit(fade * influence);
    return !((flags & 0x80000000u) != 0 && blend_in_elapsed < settings.blend_in) &&
           !(hold_end && blend_out_elapsed > 0.0f);
}

void ChannelPlayback::did_advance(AdvanceResult property) {
    flags = (flags & ~0x04000000u) | (u(property.crossed_end) << 26);
}

std::vector<AnimationAttribute> ChannelPlayback::merge_attributes(std::vector<AnimationAttribute> base,
                                                                  Slice<const AnimationAttribute> channel) const {
    if (!settings.use_attributes) {
        return base;
    }
    //82B966D0: ordered union, channel wins equal names without scaling.
    std::vector<AnimationAttribute> output;
    output.reserve(base.size() + channel.size());
    std::size_t b = 0;
    std::size_t c = 0;
    while (b < base.size() && c < channel.size()) {
        if (base[b].name._0 < channel[c].name._0) {
            output.push_back(base[b]);
            b += 1;
        } else if (channel[c].name._0 < base[b].name._0) {
            output.push_back(channel[c]);
            c += 1;
        } else {
            output.push_back(channel[c]);
            b += 1;
            c += 1;
        }
    }
    output.insert(output.end(), base.begin() + std::ptrdiff_t(b), base.end());
    output.insert(output.end(), channel.begin() + c, channel.end());
    return output;
}

}  // namespace s3::animation::channel_playback
