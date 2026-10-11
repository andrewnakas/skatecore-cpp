// Port of skate-core/src/animation/playback_parameters.rs.
//! PlayAnimation's parameter refresh82BB4760 and settable queue82D19100.
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "s3/core/animation/output/attributes.hpp"
#include "s3/core/animation/slice.hpp"
#include "s3/core/rs.hpp"

namespace s3::animation::playback_parameters {

using output::attributes::AnimationAttribute;
using output::attributes::AttributeName;

/// Rust `enum ParameterSource`; each variant is a nested struct held in `value`.
struct ParameterSource {
    struct MotionIntent {
        std::string _0;
    };
    struct FilteredIntent {
        std::string _0;
    };
    struct LastAnimation {
        AttributeName _0;
    };
    std::variant<MotionIntent, FilteredIntent, LastAnimation> value;
    ParameterSource(MotionIntent v) : value(std::move(v)) {}
    ParameterSource(FilteredIntent v) : value(std::move(v)) {}
    ParameterSource(LastAnimation v) : value(v) {}
};
S3_DERIVE_EQ(ParameterSource::MotionIntent, _0)
S3_DERIVE_EQ(ParameterSource::FilteredIntent, _0)
S3_DERIVE_EQ(ParameterSource::LastAnimation, _0)
S3_DERIVE_EQ(ParameterSource, value)

struct SettableAttribute {
    AttributeName name;
    float value = 0.0f;
    bool normalized = false;
    std::int32_t sequence_id = 0;
};
S3_DERIVE_EQ(SettableAttribute, name, value, normalized, sequence_id)

/// Gameplay providers are queried when a node executes, preserving visibility
/// of earlier same-tick graph operations and the last animation tree output.
struct ParameterInputs {
    virtual ~ParameterInputs() = default;
    virtual std::optional<float> motion_intent(std::string_view name) const = 0;
    virtual std::optional<float> filtered_intent(std::string_view name) const = 0;
    virtual rs::Result<std::optional<AnimationAttribute>, std::string> last_attribute(AttributeName name) = 0;
};

struct AttributeSink {
    virtual ~AttributeSink() = default;
    virtual void set_attribute(SettableAttribute attribute) = 0;
};

struct SettableAttributes : AttributeSink {
    Slice<const SettableAttribute> entries() const { return entries_; }
    void clear() { entries_.clear(); }
    void set_attribute(SettableAttribute attribute) override {
        for (auto& entry : entries_) {
            if (entry.name == attribute.name && entry.sequence_id == attribute.sequence_id) {
                entry = attribute;
                return;
            }
        }
        entries_.push_back(attribute);
    }

private:
    std::vector<SettableAttribute> entries_;
};

struct PlaybackParameter {
    ParameterSource source;
    std::optional<AttributeName> rename;
    std::optional<float> default_value;
    bool normalized = false;

    /// Last-animation parameters execute only on Begin. Existing non-scalar
    /// attributes suppress the default too: native does not reinterpret them.
    rs::Result<rs::Unit, std::string> update(bool beginning, ParameterInputs& inputs,
                                             AttributeSink& output) const;
};
S3_DERIVE_EQ(PlaybackParameter, source, rename, default_value, normalized)

/// Six-word intent/animation identifiers use the same native six-character
/// chunks as the five-word attribute encoder, but retain up to36 input bytes.
std::array<std::uint32_t, 6> intent_key(std::string_view text);

}  // namespace s3::animation::playback_parameters
