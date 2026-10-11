// Port of skate-game/src/graph_host/motion_animation.rs.
//! Stock tree ownership and persistent playback used directly by MotionHost.
//
// The Rust child modules `selection_space_host`, `stock_clip_query` (impl blocks of
// MotionAnimation) and `grind_owner` (#[path = "motion_grind/animation_owner.rs"]) are declared
// here as members and defined in motion_animation/<module>.cpp; `tree_builder` is a free
// function in motion_animation/tree_builder.hpp.
#pragma once
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "s3/core/animation/channel_playback.hpp"
#include "s3/core/animation/clip_clock.hpp"
#include "s3/core/animation/output/attributes.hpp"
#include "s3/core/animation/playback.hpp"
#include "s3/core/animation/playback_attributes.hpp"
#include "s3/core/animation/playback_parameters.hpp"
#include "s3/core/animation/playback_tree.hpp"
#include "s3/core/animation/posture.hpp"
#include "s3/core/animation/slice.hpp"
#include "s3/core/graph/intents.hpp"
#include "s3/core/rs.hpp"
#include "s3/data/animation_metadata.hpp"
#include "s3/game/graph_host/motion_channels.hpp"
#include "s3/game/graph_host/motion_stock_gameplay.hpp"
#include "s3/game/graph_host/outputs.hpp"
#include "s3/game/graph_host/pushing.hpp"

namespace s3::game::graph_host::motion_animation {

using s3::animation::Slice;
using s3::animation::channel_playback::ChannelSettings;
using s3::animation::clip_clock::AdvanceResult;
using s3::animation::output::attributes::AnimationAttribute;
using s3::animation::output::attributes::AttributeName;
using s3::animation::output::attributes::MotionGraphAttribute;
using s3::animation::playback::PlaybackRequest;
using s3::animation::playback::PlaybackService;
using s3::animation::playback::TransitionSettings;
using s3::animation::playback_attributes::AttributeMirror;
using s3::animation::playback_parameters::SettableAttribute;
using s3::animation::playback_parameters::SettableAttributes;
using s3::animation::playback_tree::Evaluation;
using s3::animation::playback_tree::PlaybackTree;
using s3::animation::playback_tree::PoseCommand;
using s3::animation::posture::PendingPosture;
using s3::animation::posture::PosturePose;
using s3::data::AnimationMetadata;
using s3::graph::intents::IntentMap;

/// Implements the Rust traits `ParameterInputs`, `AttributeSink`, `PlaybackService` (via
/// PlaybackService's bases) and `pushing::PushAnimationSink`. Rust-private fields are public;
/// `metadata`, `tree_attributes` and `property` gain a trailing `_` (accessor name clash).
class MotionAnimation : public PlaybackService, public pushing::PushAnimationSink {
public:
    // Private in Rust.
    AnimationMetadata metadata_;
    std::optional<PlaybackTree> current;

    motion_channels::MotionChannels channels;
    std::optional<std::string> current_name;
    IntentMap motion_intents;
    IntentMap filtered_intents;
    std::vector<MotionGraphAttribute> motion_attributes;
    std::optional<motion_stock_gameplay::GrabType> grab_type;
    std::vector<std::pair<AttributeName, AttributeName>> construction_values;
    PendingPosture posture;
    bool posture_bank_valid = false;
    ///Actual fullSkaterAnim15180 flags. AddBindPose consumes bits22/21 on
    ///construction; the owner publishes the updated flags back to the actor.
    std::optional<std::uint32_t> skater_animation_flags;
    /// SkaterAnim15188/15196/15200 at the graph publication boundary.
    std::uint32_t natural_stance = 1;
    std::uint32_t relative_stance = 0;
    std::uint32_t requested_stance = 0;
    bool reset_action_intents = false;

    // Private in Rust.
    std::shared_ptr<const AttributeMirror> attribute_mirror;
    SettableAttributes settable;
    std::vector<AnimationAttribute> tree_attributes_;
    AdvanceResult property_;

    static MotionAnimation from_metadata(AnimationMetadata metadata);

    /// `#[cfg(test)]` in Rust.
    void seek_current_fraction(float fraction);
    /// `pub(crate)`.
    const AnimationMetadata& metadata() const { return metadata_; }
    /// `#[cfg(test)] pub(super)` in Rust.
    std::optional<float> pending_parameter(AttributeName name) const;
    void accept_action_graph(const outputs::ActionGraphOutput& output);
    void reset_from_stock();
    rs::Result<rs::Unit, std::string> reset_to_given_stance();
    ///Share the already loaded stock hierarchy with playback; no frame reload.
    rs::Result<rs::Unit, std::string> set_hierarchy(Slice<const std::string> names,
                                                   Slice<const std::int32_t> mirror_indices);
    Slice<const AnimationAttribute> tree_attributes() const { return tree_attributes_; }
    AdvanceResult property() const { return property_; }
    rs::Result<float, std::string> current_time() const;
    rs::Result<float, std::string> current_length() const;
    /// MatchAirTime82BBA198..1CC: current tree v20 length, v28 SetTime.
    /// Missing current tree skips the seek, but not the caller's parameters. (`pub(super)`)
    void synchronize_air_time(float fraction);
    bool in_transition() const;
    ///JumpInto Update82BACDC0: apply pending parameters, then query the
    ///current tree with mask31 and seek to the authored attribute's begin.
    ///The caller owns the one-shot instance latch; a missing marker is a miss. (`pub(super)`)
    rs::Result<rs::Unit, std::string> jump_into(AttributeName name);
    ///825310F0: retire completed transitions before advancing; reset property
    /// once at the root. Graph evaluation, parametrization and pose evaluation
    /// remain separate calls so the actor schedule owns their meaningful order.
    void advance(float dt, float phase);
    rs::Result<rs::Unit, std::string> apply_parameters();
    rs::Result<std::vector<PoseCommand>, std::string> evaluate_pose(Evaluation parameters);
    rs::Result<rs::Unit, std::string> refresh_tree_attributes();
    void begin_graph_update() { motion_attributes.clear(); }
    void attach(std::string_view intent, AttributeName attribute, bool set);
    void emit_packet(AttributeName name, float value) { motion_attributes.push_back(MotionGraphAttribute{name, value}); }
    void set_grab_type(motion_stock_gameplay::GrabType grab_type);
    void clear_grab_type() { grab_type = std::nullopt; }
    rs::Result<PlaybackTree, std::string> build_tree(std::string_view name) const;
    rs::Result<bool, std::string> new_channel(std::string_view name, std::string_view animation,
                                              ChannelSettings settings);
    rs::Result<bool, std::string> transition_channel(std::string_view name, std::string_view animation,
                                                     ChannelSettings settings, TransitionSettings transition,
                                                     bool resurrect, bool create_missing);

    // ---- motion_animation/selection_space_host.rs (`pub(super)`) ----
    rs::Result<rs::Unit, std::string> prepare_selection_spaces(PlaybackTree& tree,
                                                              Slice<const SettableAttribute> attributes);
    // ---- motion_animation/stock_clip_query.rs (`pub(crate)`) ----
    //! Direct named-bank query82D16680, not the currently playing tree/channel.
    rs::Result<float, std::string> stock_clip_translation_z(std::string_view database,
                                                            std::string_view animation) const;
    // ---- motion_grind/animation_owner.rs (`impl motion_grind::animation::Animation`; see grind_owner.hpp) ----
    // S3 virtual48 ->825310E0 returns the current root at owner+13524.
    rs::Result<bool, std::string> query_current_attribute(AttributeName name, std::uint32_t mask,
                                                          AnimationAttribute& output) const;
    /// `Animation::intent`.
    std::optional<float> intent(std::string_view name) const { return motion_intent(name); }

    // ---- impl ParameterInputs ----
    std::optional<float> motion_intent(std::string_view name) const override;
    std::optional<float> filtered_intent(std::string_view name) const override;
    rs::Result<std::optional<AnimationAttribute>, std::string> last_attribute(AttributeName name) override;
    // ---- impl AttributeSink ----
    void set_attribute(SettableAttribute attribute) override { settable.set_attribute(attribute); }
    // ---- impl pushing::PushAnimationSink ----
    void set_attribute(pushing::SettableAttribute a) override;
    // ---- impl PlaybackService ----
    void set_construction_value(AttributeName name, AttributeName value) override;
    void set_posture_enabled(bool enabled) override { posture.set_requested(enabled); }
    rs::Result<bool, std::string> play(PlaybackRequest request) override;

    // Private in Rust.
    rs::Result<PlaybackTree, std::string> add_bind_pose(PlaybackTree motion, std::optional<PosturePose> posture);
};

}  // namespace s3::game::graph_host::motion_animation
