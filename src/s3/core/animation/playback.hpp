// Port of skate-core/src/animation/playback.rs.
//! MotionGraph PlayAnimation ctor82BB4B38, Begin82BB5188, Update82BB5670.
//! End is the verified no-op82B61BB8; leaving a graph state does not stop a tree.
#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "s3/core/animation/output/attributes.hpp"
#include "s3/core/animation/playback_parameters.hpp"
#include "s3/core/animation/slice.hpp"
#include "s3/core/rs.hpp"

namespace s3::animation::playback {

using output::attributes::AttributeName;
using playback_parameters::AttributeSink;
using playback_parameters::ParameterInputs;
using playback_parameters::PlaybackParameter;

struct TransitionSettings {
    ///1 play,2 blend,3 channelblend,4 sequence. Zero means no graph override.
    std::uint32_t kind = 0;
    float seconds = 0.0f;
    std::uint32_t under = 0;
    ///0 default,1 current frame,2 match phase,3 match frame.
    std::uint32_t matching = 0;
    bool use_channels_from_weights = false;
};
S3_DERIVE_EQ(TransitionSettings, kind, seconds, under, matching, use_channels_from_weights)

struct PlayAnimation {
    std::string animation;
    std::optional<std::string> switch_animation;
    std::optional<std::string> mirror_animation;
    std::optional<std::string> no_board_animation;
    float playback_speed = 0.0f;
    bool apply_posture = false;
    TransitionSettings transition;
    std::vector<PlaybackParameter> parameters;
};
S3_DERIVE_EQ(PlayAnimation, animation, switch_animation, mirror_animation, no_board_animation, playback_speed,
             apply_posture, transition, parameters)

struct PlaybackRequest {
    std::string animation;
    float speed = 0.0f;
    float start_time = 0.0f;
    TransitionSettings transition;
};
S3_DERIVE_EQ(PlaybackRequest, animation, speed, start_time, transition)

/// Values published by the actual skater-animation/physical interfaces. The
/// no-board bit is PhysOut+72's byte311, not a contact-derived guess.
struct PlaybackContext {
    std::optional<bool> is_switch;
    std::optional<bool> is_mirrored;
    std::optional<bool> board_available;
    AttributeName pro_skater;
    std::optional<TransitionSettings> transition_override;
};

struct PlaybackService : ParameterInputs, AttributeSink {
    virtual void set_construction_value(AttributeName name, AttributeName value) = 0;
    virtual void set_posture_enabled(bool enabled) = 0;
    /// Return false only when the native controller's Play/Blend/Sequence
    /// request fails. Unimplemented evaluation is an error, never false.
    virtual rs::Result<bool, std::string> play(PlaybackRequest request) = 0;
};

struct PlayAnimationInstance {
    bool first_update = false;

    rs::Result<rs::Unit, std::string> begin(const PlayAnimation& operation, PlaybackContext& context,
                                            PlaybackService& service);
    rs::Result<rs::Unit, std::string> update(const PlayAnimation& operation, PlaybackService& service);
    void end() {}
};

rs::Result<rs::Unit, std::string> refresh_parameters(Slice<const PlaybackParameter> parameters, bool beginning,
                                                     PlaybackService& service);

}  // namespace s3::animation::playback
