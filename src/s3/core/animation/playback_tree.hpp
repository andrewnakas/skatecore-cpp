// Port of skate-core/src/animation/playback_tree.rs.
//! Owned animation tree evaluation; stock data constructs this topology.
//
// PlaybackTree is a recursive, deep-copyable value type (Rust `#[derive(Clone)]` enum with
// `Box` children). PoseCommand and Evaluation live in playback_tree_fwd.hpp (same namespace).
#pragma once
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "s3/core/animation/playback_tree_fwd.hpp"
// Child modules / variant payloads (each only needs PlaybackTree declared).
#include "s3/core/animation/phase_blend.hpp"
#include "s3/core/animation/playback_transition.hpp"
#include "s3/core/animation/playback_tree/blend_space.hpp"
#include "s3/core/animation/playback_tree/selection_space.hpp"
//
#include "s3/core/animation/boxed.hpp"
#include "s3/core/animation/clip_clock.hpp"
#include "s3/core/animation/output/attributes.hpp"
#include "s3/core/animation/playback_attributes.hpp"
#include "s3/core/animation/playback_clip.hpp"
#include "s3/core/animation/playback_parameters.hpp"
#include "s3/core/animation/posture.hpp"
#include "s3/core/animation/slice.hpp"
#include "s3/core/rs.hpp"

namespace s3::animation::playback_tree {

using clip_clock::AdvanceResult;
using output::attributes::AnimationAttribute;
using output::attributes::AttributeName;
using playback_clip::PlaybackClip;
using playback_parameters::SettableAttribute;

/// Rust `enum PlaybackTree`; each variant is a nested struct held in `value`.
/// Tuple variants hold their payload in `_0`.
struct PlaybackTree {
    struct Clip {
        std::string name;
        PlaybackClip clip;
    };
    struct PhaseBlend {
        phase_blend::PhaseBlend _0;
    };
    struct BlendSpace {
        blend_space::BlendSpace _0;
    };
    struct SelectionSpace {
        selection_space::SelectionSpace _0;
    };
    struct Transition {
        playback_transition::PlaybackTransition _0;
    };
    ///SkaterAnim::AddBindPose82B98118, latched when a named tree is created.
    struct BindPose {
        Box<PlaybackTree> motion;
        std::optional<posture::PosturePose> posture;
        bool board_backwards = false;
        std::vector<std::uint32_t> mirror_modes;
        std::shared_ptr<const playback_attributes::AttributeMirror> attribute_mirror;
    };

    std::variant<Clip, PhaseBlend, BlendSpace, SelectionSpace, Transition, BindPose> value;

    PlaybackTree(Clip v) : value(std::move(v)) {}
    PlaybackTree(PhaseBlend v) : value(std::move(v)) {}
    PlaybackTree(BlendSpace v) : value(std::move(v)) {}
    PlaybackTree(SelectionSpace v) : value(std::move(v)) {}
    PlaybackTree(Transition v) : value(std::move(v)) {}
    PlaybackTree(BindPose v) : value(std::move(v)) {}

    float length() const;
    float time() const;
    void set_time(float time);
    void set_speed(float speed);
    void advance(float dt, float phase, AdvanceResult& property);
    rs::Result<bool, std::string> set_attributes(Slice<const SettableAttribute> attributes);
    rs::Result<std::vector<AnimationAttribute>, std::string> attributes(std::uint32_t mask) const;
    rs::Result<std::optional<AnimationAttribute>, std::string> attribute(AttributeName name,
                                                                         std::uint32_t mask) const;
    rs::Result<bool, std::string> query_attribute(AttributeName name, std::uint32_t mask,
                                                  AnimationAttribute& output) const;
    rs::Result<bool, std::string> evaluate(Evaluation parameters, bool enabled, std::vector<PoseCommand>& output);
};

}  // namespace s3::animation::playback_tree
