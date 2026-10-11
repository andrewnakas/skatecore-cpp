// Port of skate-game/src/graph_host/motion_channels.rs (out-of-line items).
#include "s3/game/graph_host/motion_channels.hpp"

#include <algorithm>
#include <iterator>

#include "s3/core/animation/playback_transition.hpp"
#include "s3/core/animation/skeleton_input/name.hpp"
#include "s3/game/graph_host/motion_animation.hpp"

namespace s3::game::graph_host::motion_channels {

using s3::animation::clip_clock::AdvanceResult;
using s3::animation::playback_parameters::intent_key;
using s3::animation::playback_transition::PlaybackTransition;

namespace {
const Channel* find(const std::vector<Channel>& channels, std::string_view name) {
    const auto key = intent_key(name);
    for (const Channel& c : channels) {
        if (intent_key(c.name) == key) return &c;
    }
    return nullptr;
}
}  // namespace

bool MotionChannels::has(std::string_view name) const { return find(channels, name) != nullptr; }

float MotionChannels::remaining(std::string_view name) const {
    const Channel* c = find(channels, name);
    if (!c) return 0.0f;
    const float value = c->tree.length() - c->tree.time();
    return value >= 0.0f ? value : 0.0f;
}

float MotionChannels::elapsed(std::string_view name) const {
    const Channel* c = find(channels, name);
    if (!c) return 0.0f;
    const float time = c->tree.time();
    const float lower = -time >= 0.0f ? 0.0f : time;
    const float length = c->tree.length();
    return length - lower >= 0.0f ? lower : length;
}

bool MotionChannels::in_transition(std::string_view name) const {
    const Channel* c = find(channels, name);
    return c != nullptr && std::holds_alternative<PlaybackTree::Transition>(c->tree.value);
}

void MotionChannels::insert(std::string name, PlaybackTree tree, ChannelSettings settings) {
    //82D1CF70..D018: greater priority lies above lesser; a new equal
    //priority lies above older channels. Store in evaluation order.
    const auto position = std::partition_point(channels.begin(), channels.end(), [&](const Channel& c) {
        return c.playback.settings.priority <= settings.priority;
    });
    channels.insert(position, Channel{std::move(name), std::move(tree), ChannelPlayback::new_(settings)});
}

void MotionChannels::end(std::string_view name) {
    if (Channel* c = find_mut(name)) c->playback.end();
}

void MotionChannels::end_with(std::string_view name, float seconds, bool from_last_frame) {
    if (Channel* c = find_mut(name)) c->playback.end_with(seconds, from_last_frame);
}

bool MotionChannels::influence(std::string_view name, float value) {
    Channel* c = find_mut(name);
    if (!c) return false;
    c->playback.influence = value;
    return true;
}

bool MotionChannels::can_transition(std::string_view name, bool resurrect) {
    Channel* c = find_mut(name);
    return c != nullptr && c->playback.can_transition(resurrect);
}

void MotionChannels::transition(std::string_view name, PlaybackTree tree, ChannelSettings settings,
                                s3::animation::playback::TransitionSettings transition, bool resurrect) {
    const auto key = intent_key(name);
    for (std::size_t index = 0; index < channels.size(); ++index) {
        if (intent_key(channels[index].name) != key) continue;
        Channel c = std::move(channels[index]);
        channels.erase(channels.begin() + static_cast<std::ptrdiff_t>(index));
        //CompleteTransition82D1DBC0 unconditionally keeps the prior target
        //when another channel transition is requested during a transition.
        PlaybackTree old = [&]() -> PlaybackTree {
            if (auto* t = std::get_if<PlaybackTree::Transition>(&c.tree.value)) return std::move(*t->_0.to);
            return std::move(c.tree);
        }();
        c.tree = PlaybackTree::Transition{PlaybackTransition::new_(std::move(old), std::move(tree), transition)};
        c.playback.transition(settings, resurrect);
        channels.insert(channels.begin() + static_cast<std::ptrdiff_t>(index), std::move(c));
        return;
    }
}

Channel* MotionChannels::find_mut(std::string_view name) {
    const auto key = intent_key(name);
    for (Channel& c : channels) {
        if (intent_key(c.name) == key) return &c;
    }
    return nullptr;
}

void MotionChannels::retire() {
    //82B96F88: complete channel transitions, remove expired channels,
    //then advance the composed tree.
    for (Channel& c : channels) {
        if (auto* t = std::get_if<PlaybackTree::Transition>(&c.tree.value)) {
            if (t->_0.complete()) {
                PlaybackTree to = *t->_0.to;
                c.tree = std::move(to);
            }
        }
    }
    channels.erase(std::remove_if(channels.begin(), channels.end(),
                                  [](const Channel& c) { return c.playback.expired(); }),
                   channels.end());
}

void MotionChannels::advance(float dt, float phase) {
    for (Channel& c : channels) {
        if (c.playback.advance(dt, c.tree.length(), c.tree.time())) {
            AdvanceResult property{false, -1.0f, -1.0f};
            c.tree.advance(dt, phase, property);
            c.playback.did_advance(property);
        }
    }
}

rs::Result<rs::Unit, std::string> MotionChannels::set_attributes(Slice<const SettableAttribute> attributes) {
    for (Channel& c : channels) {
        //82B96B50..64 forwards parameters unconditionally. Weight gates
        // pose evaluation (82B965D8), not parameter publication.
        auto r = c.tree.set_attributes(attributes);
        if (r.is_err()) return rs::make_err(r.unwrap_err());
    }
    return rs::Unit{};
}

rs::Result<std::vector<AnimationAttribute>, std::string> MotionChannels::attributes(
    std::vector<AnimationAttribute> base, std::uint32_t mask) const {
    for (const Channel& c : channels) {
        if (c.playback.settings.use_attributes) {
            auto channel = c.tree.attributes(mask);
            if (channel.is_err()) return rs::make_err(channel.unwrap_err());
            base = c.playback.merge_attributes(std::move(base), channel.unwrap());
        }
    }
    return base;
}

rs::Result<bool, std::string> MotionChannels::query_attribute(AttributeName name, std::uint32_t mask,
                                                              AnimationAttribute& output) const {
    //82B96AB0 queries channel first regardless of use_attributes or weight.
    for (auto it = channels.rbegin(); it != channels.rend(); ++it) {
        auto r = it->tree.query_attribute(name, mask, output);
        if (r.is_err()) return r;
        if (r.unwrap()) return true;
    }
    return false;
}

rs::Result<rs::Unit, std::string> MotionChannels::evaluate(Evaluation parameters, std::vector<PoseCommand>& output) {
    for (Channel& c : channels) {
        const bool enabled = c.playback.weight > 0.0f;
        auto r = c.tree.evaluate(parameters, enabled, output);
        if (r.is_err()) return rs::make_err(r.unwrap_err());
        if (r.unwrap() && enabled) {
            output.push_back(PoseCommand::ChannelBlend{c.playback.weight, false});
        }
    }
    return rs::Unit{};
}

rs::Result<rs::Unit, std::string> FakieHead::update(motion_animation::MotionAnimation& animation, bool manualing,
                                                    bool power_sliding, bool fakie) {
    using s3::animation::playback::TransitionSettings;
    using s3::animation::skeleton_input::name::encode;
    const float target = manualing ? 1.0f : power_sliding ? 0.0f : 0.5f;
    if (!was_fakie && fakie) {
        ChannelSettings settings;
        settings.priority = 0;
        settings.keep_alive = true;
        settings.mirrored = false;
        settings.speed = 1.0f;
        settings.blend_in = rs::from_bits(0x3e99999au);
        settings.hold_during_blend_in = false;
        settings.blend_out = rs::from_bits(0x3e99999au);
        settings.hold_during_blend_out = false;
        settings.use_attributes = false;
        //Actual vtable8231E128+16 is TransitionTo82D1D090.
        //The source permits both resurrection and creation when missing.
        TransitionSettings transition;
        transition.kind = 2;
        transition.seconds = rs::from_bits(0x3dcccccdu);
        transition.under = 0;
        transition.matching = 0;
        transition.use_channels_from_weights = false;
        auto r = animation.transition_channel("fakie", "B_FAKIE_CHANNEL", settings, transition, true, true);
        if (r.is_err()) return rs::make_err(r.unwrap_err());
        value = target;
    } else if (was_fakie && !fakie) {
        animation.channels.end("fakie");
    }
    was_fakie = fakie;
    if (fakie) {
        float delta = target - value;
        const float limit = rs::from_bits(0x3c23d70au);
        const float lower = -limit - delta >= 0.0f ? -limit : delta;
        delta = limit - lower >= 0.0f ? lower : limit;
        value += delta;
        animation.set_attribute(SettableAttribute{encode("torso"), value, false, -1});
    }
    return rs::Unit{};
}

}  // namespace s3::game::graph_host::motion_channels
