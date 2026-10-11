// Port of skate-core/src/animation/playback_transition.rs.
//! TU3 Sequence82B95C18 and Blend82B96058/82B961F8/82B963D8.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "s3/core/animation/boxed.hpp"
#include "s3/core/animation/clip_clock.hpp"
#include "s3/core/animation/output/attributes.hpp"
#include "s3/core/animation/playback.hpp"
#include "s3/core/animation/playback_parameters.hpp"
#include "s3/core/animation/playback_tree_fwd.hpp"
#include "s3/core/animation/slice.hpp"
#include "s3/core/rs.hpp"

namespace s3::animation::playback_transition {

using clip_clock::AdvanceResult;
using output::attributes::AnimationAttribute;
using output::attributes::AttributeName;
using playback::TransitionSettings;
using playback_parameters::SettableAttribute;
using playback_tree::Evaluation;
using playback_tree::PlaybackTree;
using playback_tree::PoseCommand;

struct PlaybackTransition {
    Box<PlaybackTree> from;
    Box<PlaybackTree> to;
    TransitionSettings settings;
    float elapsed = 0.0f;
    bool sequence_complete = false;

    static PlaybackTransition new_(PlaybackTree from, PlaybackTree to, TransitionSettings settings);
    bool complete() const;
    void advance(float dt, float phase, AdvanceResult& property);
    rs::Result<bool, std::string> set_attributes(Slice<const SettableAttribute> attributes);
    const PlaybackTree& attributes_child() const;
    rs::Result<std::vector<AnimationAttribute>, std::string> attributes(std::uint32_t mask) const;
    rs::Result<bool, std::string> query_attribute(AttributeName name, std::uint32_t mask,
                                                  AnimationAttribute& output) const;
    float weight() const;
    rs::Result<bool, std::string> evaluate(Evaluation parameters, bool enabled, std::vector<PoseCommand>& output);
};

}  // namespace s3::animation::playback_transition

// Completes PlaybackTree for includers of this header (no-op when already in progress).
#include "s3/core/animation/playback_tree.hpp"
