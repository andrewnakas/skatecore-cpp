// Port of skate-game/src/graph_host/pushing.rs.
//! MotionGraph pushing operations. The stock graph owns activation and order;
//! these handlers only mutate native push properties and animation attributes.
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "s3/core/graph/intents.hpp"
#include "s3/core/riding/push_behaviors.hpp"
#include "s3/core/rs.hpp"
#include "s3/data/state_graph.hpp"
#include "s3/game/graph_host/pushing_settings.hpp"

namespace s3::game::graph_host::pushing {

using pushing_settings::PushingSettings;
using s3::data::state_graph_attributes::Attributes;
using s3::graph::intents::IntentMap;
using s3::riding::push_behaviors::FirstPushStrength;
using s3::riding::push_behaviors::PushFootFrame;
using s3::riding::push_behaviors::PushIntents;
using s3::riding::push_behaviors::PushState;

/// Rust `enum PushOperation`; each variant is a nested struct held in `value`.
struct PushOperation {
    struct InitPush {};
    struct ComputeFirstPushStrength {};
    struct ComputeTargetCoefsFromSpeedAndStrength {
        bool regular_attributes = false;
    };
    struct ComputeRepushDeadline {
        bool regular_attributes = false;
    };
    struct SetPushCoefs {
        bool on_first_update_only = false;
    };
    struct PushCycle {
        std::string new_push_name;
    };
    struct PushOut {
        bool is_right_foot = false;
    };

    std::variant<InitPush, ComputeFirstPushStrength, ComputeTargetCoefsFromSpeedAndStrength, ComputeRepushDeadline,
                 SetPushCoefs, PushCycle, PushOut>
        value;

    PushOperation(InitPush v) : value(v) {}
    PushOperation(ComputeFirstPushStrength v) : value(v) {}
    PushOperation(ComputeTargetCoefsFromSpeedAndStrength v) : value(v) {}
    PushOperation(ComputeRepushDeadline v) : value(v) {}
    PushOperation(SetPushCoefs v) : value(v) {}
    PushOperation(PushCycle v) : value(std::move(v)) {}
    PushOperation(PushOut v) : value(v) {}

    /// Defaults verified in TU3 factories82BC7ED0/7F98/80B8/8180/8248.
    /// None means this is another operation, for the enclosing host to resolve.
    static std::optional<PushOperation> from_attributes(const Attributes& attributes);
};
S3_DERIVE_EQ_EMPTY(PushOperation::InitPush)
S3_DERIVE_EQ_EMPTY(PushOperation::ComputeFirstPushStrength)
S3_DERIVE_EQ(PushOperation::ComputeTargetCoefsFromSpeedAndStrength, regular_attributes)
S3_DERIVE_EQ(PushOperation::ComputeRepushDeadline, regular_attributes)
S3_DERIVE_EQ(PushOperation::SetPushCoefs, on_first_update_only)
S3_DERIVE_EQ(PushOperation::PushCycle, new_push_name)
S3_DERIVE_EQ(PushOperation::PushOut, is_right_foot)
S3_DERIVE_EQ(PushOperation, value)

/// The scalar SettableAtt values used here are untimed (begin/end=-1),
/// kind0/status6, as published by82D19100. `normalized` is the control flag
/// consumed by the animation tree, not a request to clamp the stored scalar.
struct SettableAttribute {
    std::string_view name;  // &'static str
    float value = 0.0f;
    bool normalized = false;
    std::int32_t sequence_id = 0;
};
S3_DERIVE_EQ(SettableAttribute, name, value, normalized, sequence_id)

struct PushAnimationSink {
    virtual ~PushAnimationSink() = default;
    /// Apply in call order to the active animation controller. Upsert the first
    /// matching (name,sequence_id), or append, following TU382D19100.
    virtual void set_attribute(SettableAttribute attribute) = 0;
};
/// `impl PushAnimationSink for Vec<SettableAttribute>`.
inline void set_attribute(std::vector<SettableAttribute>& self, SettableAttribute attribute) {
    for (SettableAttribute& existing : self) {
        if (existing.name == attribute.name && existing.sequence_id == attribute.sequence_id) {
            existing = attribute;
            return;
        }
    }
    self.push_back(attribute);
}
inline void set_attribute(PushAnimationSink& self, SettableAttribute attribute) { self.set_attribute(attribute); }

struct PushContext {
    const PushingSettings& settings;
    PushState& shared;
    /// The current MG list: Pushing carries accumulated AG-held seconds.
    const IntentMap& motion_intents;
    float forward_speed = 0.0f;
    float delta_seconds = 0.0f;
    float time_since_teleport = 0.0f;
    /// ISkaterAnim::GetIsSwitch (TU382B97128), distinct from mirror/natural stance.
    bool is_switch = false;
    /// None only for an actor without a physical interface, as in82595B28.
    /// A physical actor must supply its actual skeleton/reckoning outputs.
    std::optional<PushFootFrame> foot_frame;

    PushIntents intents(std::string_view configured_name) const {
        const float* pushing = motion_intents.get("Pushing");
        return PushIntents{pushing ? std::optional<float>(*pushing) : std::nullopt,
                           motion_intents.contains_key("NewPush"), motion_intents.contains_key(configured_name)};
    }
};

namespace detail {
template <class Sink>
inline void publish(Sink& sink, std::string_view name, float value, bool normalized) {
    set_attribute(sink, SettableAttribute{name, value, normalized, -1});
}
}  // namespace detail

/// Allocate one per graph activation. Its first-push/cycle data must not be
/// shared between separate active nodes, nor retained across a fresh activation.
struct PushInstance {
    PushOperation operation;
    std::optional<FirstPushStrength> first_strength;
    std::optional<s3::riding::push_behaviors::PushCycle> cycle;
    bool first_update = false;

    static PushInstance new_(PushOperation operation) {
        return PushInstance{std::move(operation), std::nullopt, std::nullopt, false};
    }

    /// `sink`: `std::vector<SettableAttribute>` or any `PushAnimationSink`.
    template <class Sink>
    void begin(PushContext& context, Sink& sink) {
        if (std::holds_alternative<PushOperation::InitPush>(operation.value)) {
            context.shared.initialize_push();  //82BAD5B8
        } else if (std::holds_alternative<PushOperation::ComputeFirstPushStrength>(operation.value)) {
            first_strength =
                FirstPushStrength::begin(context.time_since_teleport, context.settings.teleport_window);  //82BAD6E8
        } else if (std::holds_alternative<PushOperation::SetPushCoefs>(operation.value)) {
            first_update = true;  //82BACEF0
        } else if (const auto* op = std::get_if<PushOperation::PushCycle>(&operation.value)) {
            const PushIntents intents = context.intents(op->new_push_name);
            cycle = s3::riding::push_behaviors::PushCycle::begin(context.shared, context.settings.curves, intents);
        } else if (const auto* op = std::get_if<PushOperation::PushOut>(&operation.value)) {
            //82BAE210 ->82595B28; output writes are begin-only and ordered X,Y.
            const std::array<float, 2> xy =
                context.foot_frame ? context.foot_frame->out_distance(op->is_right_foot) : std::array<float, 2>{};
            detail::publish(sink, "OutDistanceX", xy[0], false);
            detail::publish(sink, "OutDistanceY", xy[1], false);
        }
    }

    template <class Sink>
    rs::Result<rs::Unit, std::string> update(PushContext& context, Sink& sink) {
        if (std::holds_alternative<PushOperation::ComputeFirstPushStrength>(operation.value)) {
            if (!first_strength) return rs::make_err(std::string("ComputeFirstPushStrength updated before Begin"));
            const float* pushing = context.motion_intents.get("Pushing");
            first_strength->update(context.shared, context.settings.curves,
                                   pushing ? std::optional<float>(*pushing) : std::nullopt, context.forward_speed,
                                   context.delta_seconds);
        } else if (const auto* op =
                       std::get_if<PushOperation::ComputeTargetCoefsFromSpeedAndStrength>(&operation.value)) {
            context.shared.target = context.settings.attributes(op->regular_attributes, context.is_switch)
                                        .target(context.forward_speed, context.shared.current_push_dv);  //82BAD258/82BAD260
        } else if (const auto* op = std::get_if<PushOperation::SetPushCoefs>(&operation.value)) {
            if (!op->on_first_update_only || first_update) {
                context.shared.current = context.settings.curves.update_blend(
                    context.shared.current, context.shared.target, context.forward_speed,
                    context.delta_seconds);  //82BACF50
                const auto values = context.shared.current;
                detail::publish(sink, "HStr_Vel_B", values.hstr_vel_b, true);
                detail::publish(sink, "LStr_Vel_B", values.lstr_vel_b, true);
                detail::publish(sink, "Vel_E", values.vel_e, true);
            }
            first_update = false;  //82BACF00
        } else if (const auto* op = std::get_if<PushOperation::PushCycle>(&operation.value)) {
            const PushIntents intents = context.intents(op->new_push_name);
            if (!cycle) return rs::make_err(std::string("PushCycle updated before Begin"));
            cycle->update(context.shared, context.settings.curves, intents, context.forward_speed,
                          context.settings.maximum_holding_acceleration);
        }
        return rs::Unit{};
    }

    void end(PushContext& context) {
        if (const auto* op = std::get_if<PushOperation::ComputeRepushDeadline>(&operation.value)) {
            context.shared.out_factor =
                context.settings.attributes(op->regular_attributes, context.is_switch)
                    .out_factor(context.forward_speed, context.shared.current_push_dv,
                                context.settings.out_speed_weight, context.settings.maximum_out_factor);  //82BADA90
        }
    }
};

}  // namespace s3::game::graph_host::pushing
