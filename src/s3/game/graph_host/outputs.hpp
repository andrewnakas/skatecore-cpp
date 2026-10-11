// Port of skate-game/src/graph_host/outputs.rs.
//! Explicit graph-phase transport.
//!
//! These records are the typed boundary between the two native graph phases.
//! Graph hosts publish complete records. Unsupported branch execution keeps
//! the existing explicit runtime error policy.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "s3/core/animation/output/attributes.hpp"
#include "s3/core/animation/slice.hpp"
#include "s3/core/graph/intents.hpp"
#include "s3/core/player/state.hpp"
#include "s3/core/rs.hpp"

namespace s3::game::graph_host::outputs {

using s3::animation::output::attributes::AnimationAttribute;
using s3::graph::intents::IntentMap;
using s3::player::state::PhysicalStateId;

/// Private in Rust.
struct AuthoredSignals {
    IntentMap values;

    static AuthoredSignals from_values(const IntentMap& values) { return AuthoredSignals{values}; }
    IntentMap into_values() const { return values; }
    std::optional<float> get(std::string_view name) const {
        const float* value = values.get(name);
        return value ? std::optional<float>(*value) : std::nullopt;
    }
    bool contains(std::string_view name) const { return values.contains_key(name); }
};
S3_DERIVE_EQ(AuthoredSignals, values)

struct WipeoutControls {
    bool request = false;
    std::array<float, 2> air_body_tweak{};
    std::optional<std::array<float, 2>> gesture;
};
S3_DERIVE_EQ(WipeoutControls, request, air_body_tweak, gesture)

struct BoardControls {
    bool drop_requested = false;
    bool throw_requested = false;
    bool retrieve_requested = false;
};
S3_DERIVE_EQ(BoardControls, drop_requested, throw_requested, retrieve_requested)

struct ActionControls {
    WipeoutControls wipeout;
    BoardControls board;
    /// Private `authored_values` field (renamed: clashes with the method).
    AuthoredSignals authored_values_;

    bool has(std::string_view name) const { return authored_values_.contains(name); }
    std::optional<float> value(std::string_view name) const { return authored_values_.get(name); }
    IntentMap authored_values() const { return authored_values_.into_values(); }
};
S3_DERIVE_EQ(ActionControls, wipeout, board, authored_values_)

struct MotionEffects {
    AuthoredSignals values;

    static MotionEffects from_values(const IntentMap& values) {
        return MotionEffects{AuthoredSignals::from_values(values)};
    }
    void apply_to(IntentMap& destination) const { destination = values.into_values(); }
};
S3_DERIVE_EQ(MotionEffects, values)

struct ActionInput {
    AuthoredSignals values;

    static ActionInput from_values(const IntentMap& values) {
        return ActionInput{AuthoredSignals::from_values(values)};
    }
    IntentMap into_values() const { return values.into_values(); }
};
S3_DERIVE_EQ(ActionInput, values)

struct PriorMotionState {
    AuthoredSignals values;

    static PriorMotionState from_values(const IntentMap& values) {
        return PriorMotionState{AuthoredSignals::from_values(values)};
    }
    IntentMap into_values() const { return values.into_values(); }
};
S3_DERIVE_EQ(PriorMotionState, values)

/// Tick-scoped graph diagnostics. Native graph failures are actionable, but a
/// long-lived host must not grow an error vector without bound when an authored
/// graph repeatedly reaches an unsupported node.
class GraphDiagnostics {
public:
    static constexpr std::size_t CAPACITY = 64;

    void push(std::string message) {
        if (messages_.size() < CAPACITY) {
            messages_.push_back(std::move(message));
        } else {
            overflowed_ = true;
        }
    }
    bool is_empty() const { return messages_.empty() && !overflowed_; }
    void clear() {
        messages_.clear();
        overflowed_ = false;
    }
    std::string join(std::string_view separator) const {
        std::string message;
        for (std::size_t i = 0; i < messages_.size(); ++i) {
            if (i) message += separator;
            message += messages_[i];
        }
        if (overflowed_) {
            if (!message.empty()) message += separator;
            message += "graph diagnostics truncated after 64 entries";
        }
        return message;
    }

private:
    std::vector<std::string> messages_;
    bool overflowed_ = false;
};

/// Load-time declaration of what a stock graph can execute in this build.
/// This is a diagnostic for a failed load, not a runtime compatibility mode.
struct GraphCapabilityReport {
    std::size_t supported_operations = 0;
    std::vector<std::string> unsupported_operations;
    std::vector<std::string> unsupported_conditions;
    std::vector<std::string> unsupported_hooks;

    rs::Result<rs::Unit, std::string> require_complete(std::string_view graph) const;
};
S3_DERIVE_EQ(GraphCapabilityReport, supported_operations, unsupported_operations, unsupported_conditions,
             unsupported_hooks)

struct TurningOutput {
    float turn = 0.0f;
    float raw_turn = 0.0f;
    float hard_turn = 0.0f;
};
S3_DERIVE_EQ(TurningOutput, turn, raw_turn, hard_turn)

/// Typed graph effects with declared destinations.
struct GraphEffects {
    std::vector<PhysicalStateId> state_requests;
};
S3_DERIVE_EQ(GraphEffects, state_requests)

struct ActionGraphInput {
    std::uint64_t tick = 0;
    ActionInput controls;
    PriorMotionState prior_motion;
    std::vector<AnimationAttribute> animation_attributes;
};

struct ActionGraphOutput {
    std::uint64_t tick = 0;
    ActionControls controls;
    MotionEffects motion_effects;
    TurningOutput turning;
    std::vector<AnimationAttribute> animation_attributes;
    GraphEffects effects;

    static ActionGraphOutput from_host(std::uint64_t tick, const IntentMap& action_intents,
                                       const IntentMap& motion_intents,
                                       s3::animation::Slice<const AnimationAttribute> animation_attributes);
};

struct MotionGraphInput {
    std::uint64_t tick = 0;
    ActionGraphOutput action;
};

}  // namespace s3::game::graph_host::outputs
