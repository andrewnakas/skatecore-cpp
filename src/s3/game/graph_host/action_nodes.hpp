// Port of skate-game/src/graph_host/action_nodes.rs.
//! Authored ActionGraph intent behavior configuration.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "s3/core/graph/conditions.hpp"
#include "s3/core/rs.hpp"
#include "s3/data/result.hpp"
#include "s3/data/state_graph.hpp"
#include "s3/game/graph_host/action_conditions.hpp"
#include "s3/game/graph_host/compat/input_gesture_catalog.hpp"

namespace s3::game::graph_host::action_nodes {

using s3::data::state_graph_attributes::Attributes;
using s3::data::state_graph_binding::Node;
using s3::data::state_graph_binding::OperationKind;

/// Rust `{:?}` of `OperationKind`.
inline const char* debug_kind(OperationKind kind) {
    switch (kind) {
        case OperationKind::Behavior: return "Behavior";
        case OperationKind::Condition: return "Condition";
        case OperationKind::Hook: return "Hook";
    }
    return "";
}

struct Parameter {
    std::optional<std::string> name;
    std::optional<std::string> mg_intent;
    std::optional<std::string> mg_intent_mag;
    std::optional<std::string> mg_intent_angle;
    std::optional<std::string> ag_intent;
    std::optional<std::string> text;
    std::optional<std::uint32_t> float_bits;
    std::optional<std::uint8_t> boolean_byte;
    std::optional<float> default_value;
    std::optional<float> scale;
    bool on_update = false;
    std::array<std::uint32_t, 4> filters{};
    std::uint32_t angle_filter = 0;
    bool negate_on_mirror = false;

    /// Private in Rust.
    static Parameter from_attributes(const Attributes& attributes);
};
S3_DERIVE_EQ(Parameter, name, mg_intent, mg_intent_mag, mg_intent_angle, ag_intent, text, float_bits, boolean_byte,
             default_value, scale, on_update, filters, angle_filter, negate_on_mirror)

/// Private in Rust. TU382F847C0..82F84850 initialize the names consumed by82BA0A48.
std::uint32_t filter(const std::string* value);

/// Rust `enum ActionOperation`; each variant is a nested struct held in `value`.
struct ActionOperation {
    struct Condition {
        s3::graph::conditions::ActionCondition _0;
    };
    struct ExtraCondition {
        action_conditions::ActionCondition _0;
    };
    struct PhysicsRequestsDismount {};
    struct IsLandingOnBoard {};
    struct CreateMgIntent {};
    struct CreateMgTimeIntent {};
    struct CreateConstMgIntent {};
    struct BoardAdjust {};
    struct CreateTrickIntentFromGesture {
        s3::game::input::gesture_catalog::Group group = s3::game::input::gesture_catalog::Group::Square;
        std::optional<std::string> override_name;
    };
    struct JuiceHook {};
    struct BodyFlippingSignal {};
    /// Diagnostic presentation is owned by this host and has no intent effect.
    struct PrintText {
        std::string _0;
    };
    struct Unsupported {
        OperationKind kind = OperationKind::Behavior;
        std::string name;
    };

    using Value = std::variant<Condition, ExtraCondition, PhysicsRequestsDismount, IsLandingOnBoard, CreateMgIntent,
                               CreateMgTimeIntent, CreateConstMgIntent, BoardAdjust, CreateTrickIntentFromGesture,
                               JuiceHook, BodyFlippingSignal, PrintText, Unsupported>;
    Value value;

    template <class V, class = std::enable_if_t<std::is_constructible_v<Value, V>>>
    ActionOperation(V v) : value(std::move(v)) {}

    template <class V>
    bool is() const {
        return std::holds_alternative<V>(value);
    }
};
S3_DERIVE_EQ(ActionOperation::Condition, _0)
S3_DERIVE_EQ(ActionOperation::ExtraCondition, _0)
S3_DERIVE_EQ_EMPTY(ActionOperation::PhysicsRequestsDismount)
S3_DERIVE_EQ_EMPTY(ActionOperation::IsLandingOnBoard)
S3_DERIVE_EQ_EMPTY(ActionOperation::CreateMgIntent)
S3_DERIVE_EQ_EMPTY(ActionOperation::CreateMgTimeIntent)
S3_DERIVE_EQ_EMPTY(ActionOperation::CreateConstMgIntent)
S3_DERIVE_EQ_EMPTY(ActionOperation::BoardAdjust)
S3_DERIVE_EQ(ActionOperation::CreateTrickIntentFromGesture, group, override_name)
S3_DERIVE_EQ_EMPTY(ActionOperation::JuiceHook)
S3_DERIVE_EQ_EMPTY(ActionOperation::BodyFlippingSignal)
S3_DERIVE_EQ(ActionOperation::PrintText, _0)
S3_DERIVE_EQ(ActionOperation::Unsupported, kind, name)
S3_DERIVE_EQ(ActionOperation, value)

struct UnsupportedOperation {
    OperationKind kind = OperationKind::Behavior;
    std::string name;
};
S3_DERIVE_EQ(UnsupportedOperation, kind, name)

struct ActionInstance {
    ActionOperation operation;
    Parameter config;
    std::vector<Parameter> parameters;

    std::optional<UnsupportedOperation> unsupported() const {
        if (const auto* u = std::get_if<ActionOperation::Unsupported>(&operation.value)) {
            return UnsupportedOperation{u->kind, u->name};
        }
        return std::nullopt;
    }
};
S3_DERIVE_EQ(ActionInstance, operation, config, parameters)

/// Rust tuple struct `ActionFactoryError(pub String)`; Display writes `_0`.
struct ActionFactoryError {
    std::string _0;
    const std::string& to_string() const { return _0; }
};
S3_DERIVE_EQ(ActionFactoryError, _0)

/// `impl OperationFactory for ActionFactory`, shaped for
/// `s3::data::state_graph_binding::Binding::instantiate_operations` (its error channel is the
/// Display text of `ActionFactoryError`).
struct ActionFactory {
    static ActionOperation operation(OperationKind kind, std::string_view name);

    s3::data::Result<std::optional<ActionInstance>> create(OperationKind kind, Node parent,
                                                           const Attributes& attributes);
    s3::data::Status add_parameter(ActionInstance& instance, const Attributes& attributes);
};

struct ActionInstances {
    std::vector<ActionInstance> operations;

    static ActionInstances new_(std::vector<ActionInstance> operations) {
        return ActionInstances{std::move(operations)};
    }
    /// nullptr = None.
    const ActionInstance* get(std::size_t operation) const {
        return operation < operations.size() ? &operations[operation] : nullptr;
    }
};

}  // namespace s3::game::graph_host::action_nodes
