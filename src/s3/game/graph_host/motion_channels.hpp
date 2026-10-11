// Port of skate-game/src/graph_host/motion_channels.rs.
//! Owned counterpart of ChannelAnimatable's ordered persistent overlays.
#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "s3/core/animation/channel_playback.hpp"
#include "s3/core/animation/clip_clock.hpp"
#include "s3/core/animation/output/attributes.hpp"
#include "s3/core/animation/playback.hpp"
#include "s3/core/animation/playback_parameters.hpp"
#include "s3/core/animation/playback_tree.hpp"
#include "s3/core/animation/slice.hpp"
#include "s3/core/rs.hpp"

namespace s3::game::graph_host::motion_animation {
class MotionAnimation;
}

namespace s3::game::graph_host::motion_channels {

using s3::animation::Slice;
using s3::animation::channel_playback::ChannelPlayback;
using s3::animation::channel_playback::ChannelSettings;
using s3::animation::output::attributes::AnimationAttribute;
using s3::animation::output::attributes::AttributeName;
using s3::animation::playback_parameters::SettableAttribute;
using s3::animation::playback_tree::Evaluation;
using s3::animation::playback_tree::PlaybackTree;
using s3::animation::playback_tree::PoseCommand;

/// Private in Rust.
struct Channel {
    std::string name;
    PlaybackTree tree;
    ChannelPlayback playback;
};

class MotionChannels {
public:
    void reset_from_stock() { channels.clear(); }
    /// `pub(super)`. `prepare`: `FnMut(&mut PlaybackTree) -> Result<(), String>`.
    template <class F>
    rs::Result<rs::Unit, std::string> prepare_trees(F&& prepare) {
        for (Channel& channel : channels) {
            // Channel::SetAttributes82B96B20 reaches the child even before
            // fade-in becomes visible. Prepare its selection subtree likewise.
            rs::Result<rs::Unit, std::string> r = prepare(channel.tree);
            if (r.is_err()) return r;
        }
        return rs::Unit{};
    }
    bool has(std::string_view name) const;
    ///82D1D5F8: missing channel returns zero; clamp remaining child time.
    float remaining(std::string_view name) const;
    ///82D1D4B8: clamp the actual child time to [0, child length].
    float elapsed(std::string_view name) const;
    ///82B96FF0 reads Channel136, set/cleared by Start/CompleteTransition.
    bool in_transition(std::string_view name) const;
    void insert(std::string name, PlaybackTree tree, ChannelSettings settings);
    void end(std::string_view name);
    void end_with(std::string_view name, float seconds, bool from_last_frame);
    bool influence(std::string_view name, float value);
    bool can_transition(std::string_view name, bool resurrect);
    void transition(std::string_view name, PlaybackTree tree, ChannelSettings settings,
                    s3::animation::playback::TransitionSettings transition, bool resurrect);
    void retire();
    void advance(float dt, float phase);
    rs::Result<rs::Unit, std::string> set_attributes(Slice<const SettableAttribute> attributes);
    rs::Result<std::vector<AnimationAttribute>, std::string> attributes(std::vector<AnimationAttribute> base,
                                                                         std::uint32_t mask) const;
    rs::Result<bool, std::string> query_attribute(AttributeName name, std::uint32_t mask,
                                                  AnimationAttribute& output) const;
    rs::Result<rs::Unit, std::string> evaluate(Evaluation parameters, std::vector<PoseCommand>& output);

    // Private in Rust.
    Channel* find_mut(std::string_view name);
    std::vector<Channel> channels;
};

///FakieHeadChannel82BAC778, instance82BACA30; both instance fields seed0.
struct FakieHead {
    // Private in Rust.
    float value = 0.0f;
    bool was_fakie = false;

    rs::Result<rs::Unit, std::string> update(motion_animation::MotionAnimation& animation, bool manualing,
                                             bool power_sliding, bool fakie);
};

}  // namespace s3::game::graph_host::motion_channels
