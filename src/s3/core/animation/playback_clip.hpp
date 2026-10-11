// Port of skate-core/src/animation/playback_clip.rs.
//! Stateful direct Andale Clip clock and attribute evaluation.
//! Init827B8AB0, AddTime82D25A00, GetAttributes82D25E30,
//! Attribute::Init82D164F8 and scalar curve lookup82D193E0.
#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "s3/core/animation/clip_clock.hpp"
#include "s3/core/animation/output/attributes.hpp"
#include "s3/core/animation/slice.hpp"
#include "s3/core/rs.hpp"

namespace s3::animation::playback_clip {

using clip_clock::AdvanceResult;
using clip_clock::ClipClock;
using output::attributes::AnimationAttribute;
using output::attributes::AttributeName;
using output::attributes::AttributePayload;

struct ClipAttribute {
    AttributeName name;
    std::uint8_t kind = 0;
    float begin = 0.0f;
    float end = 0.0f;
    std::vector<std::uint32_t> payload;
};

struct PlaybackClip {
    ClipClock clock;
    std::vector<ClipAttribute> attributes_;

    static PlaybackClip new_(float frames, float fps, float base_speed, std::uint32_t flags,
                             std::vector<ClipAttribute> attributes);
    /// Animatable825310F0 resets the property before every AddTime call.
    AdvanceResult advance(float dt, float phase);
    /// Ordered GetAttributes. The untimed bulk path passes status4, unlike
    /// GetAttributeStatus's untimed status6; its times are multiplied by length.
    rs::Result<std::vector<AnimationAttribute>, std::string> attributes(std::uint32_t mask) const;
    ///82D25CA0 uses status6 for untimed records and returns first valid match.
    rs::Result<std::optional<AnimationAttribute>, std::string> attribute(AttributeName name,
                                                                         std::uint32_t mask) const;
    rs::Result<AnimationAttribute, std::string> materialize(const ClipAttribute& attribute,
                                                            std::uint8_t status) const;
};

/// The curve record is an opaque word, point count, then(x,y) pairs. Native
/// binary search interpolates the final crossed bounds with a fused multiply.
rs::Result<float, std::string> sample_curve(Slice<const std::uint32_t> words, float time);

}  // namespace s3::animation::playback_clip
