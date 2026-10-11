// Port of skate-core/src/animation/phase_blend.rs.
//! Andale PhaseBlend, TU3 82D24C40..82D25808; construction 82D1B67C.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "s3/core/animation/clip_clock.hpp"
#include "s3/core/animation/output/attributes.hpp"
#include "s3/core/animation/playback_parameters.hpp"
#include "s3/core/animation/playback_tree_fwd.hpp"
#include "s3/core/animation/slice.hpp"
#include "s3/core/rs.hpp"

namespace s3::animation::phase_blend {

using clip_clock::AdvanceResult;
using output::attributes::AnimationAttribute;
using output::attributes::AttributeName;
using playback_parameters::SettableAttribute;
using playback_tree::Evaluation;
using playback_tree::PlaybackTree;
using playback_tree::PoseCommand;

struct PhaseBlend {
    AttributeName parameter;
    std::vector<PlaybackTree> children;
    std::vector<std::size_t> order;
    std::vector<float> values;
    std::size_t left = 0;
    std::size_t right = 0;
    float weight = 0.0f;
    float parameter_value = 0.0f;
    bool normalized = false;
    float cull_threshold = 0.0f;
    float length = 0.0f;
    float time = 0.0f;

    static rs::Result<PhaseBlend, std::string> new_(AttributeName parameter, std::vector<PlaybackTree> children);
    void set_time(float time);
    float phase() const;
    void update_length(float phase);
    void set_speed(float speed);
    void advance(float dt, float phase, AdvanceResult& property);
    rs::Result<bool, std::string> set_attributes(Slice<const SettableAttribute> attributes);
    rs::Result<std::vector<AnimationAttribute>, std::string> attributes(std::uint32_t mask) const;
    rs::Result<bool, std::string> query_attribute(AttributeName name, std::uint32_t mask,
                                                  AnimationAttribute& output) const;
    rs::Result<bool, std::string> evaluate(Evaluation parameters, bool enabled, std::vector<PoseCommand>& output);
};

}  // namespace s3::animation::phase_blend

// Completes PlaybackTree for includers of this header (no-op when already in progress).
#include "s3/core/animation/playback_tree.hpp"
