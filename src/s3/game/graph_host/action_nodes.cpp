// Port of skate-game/src/graph_host/action_nodes.rs.
#include "s3/game/graph_host/action_nodes.hpp"

#include "s3/game/graph_host/condition_nodes.hpp"

namespace s3::game::graph_host::action_nodes {

namespace {
std::optional<std::string> owned(const std::string* text) {
    return text ? std::optional<std::string>(*text) : std::nullopt;
}
}  // namespace

Parameter Parameter::from_attributes(const Attributes& attributes) {
    Parameter p;
    p.name = owned(attributes.text("name"));
    p.mg_intent = owned(attributes.text("MGIntent"));
    p.mg_intent_mag = owned(attributes.text("MGIntentMag"));
    p.mg_intent_angle = owned(attributes.text("MGIntentAngle"));
    p.ag_intent = owned(attributes.text("AGIntent"));
    p.text = owned(attributes.text("value"));
    const auto* value = attributes.get("value");
    p.float_bits = value ? std::optional<std::uint32_t>(value->float_bits) : std::nullopt;
    p.boolean_byte = value ? std::optional<std::uint8_t>(value->boolean_byte) : std::nullopt;
    const auto* default_value = attributes.get("defaultValue");
    p.default_value = default_value ? std::optional<float>(rs::from_bits(default_value->float_bits)) : std::nullopt;
    const auto* scale = attributes.get("scale");
    p.scale = scale ? std::optional<float>(rs::from_bits(scale->float_bits)) : std::nullopt;
    // Both TU3 constructors82BA23C0/82BA1768 default this to true.
    p.on_update = attributes.boolean_byte("onUpdate", 1) != 0;
    const std::string* filter1 = attributes.text("filter1");
    p.filters = {
        filter(filter1 ? filter1 : attributes.text("filter")),
        filter(attributes.text("filter2")),
        filter(attributes.text("fakieFilter")),
        filter(attributes.text("mirrorFilter")),
    };
    p.angle_filter = filter(attributes.text("angleFilter"));
    //BoardAdjust82BA2A90 passes true as the constructor default.
    p.negate_on_mirror = attributes.boolean_byte("negateOnMirror", 1) != 0;
    return p;
}

std::uint32_t filter(const std::string* value) {
    if (value == nullptr) return 0;
    if (*value == "negate") return 1;
    if (*value == "abs") return 2;
    if (*value == "oneMinus") return 3;
    if (*value == "clamp") return 4;
    if (*value == "angleFlip") return 5;
    if (*value == "angleRot90") return 6;
    if (*value == "angleRotN90") return 7;
    return 0;
}

ActionOperation ActionFactory::operation(OperationKind kind, std::string_view name) {
    if (kind == OperationKind::Condition && name == "PhysicsRequestsDismount")
        return ActionOperation::PhysicsRequestsDismount{};
    if (kind == OperationKind::Condition && name == "IsLandingOnBoard") return ActionOperation::IsLandingOnBoard{};
    if (kind == OperationKind::Behavior && name == "CreateMGIntentFromAGIntent")
        return ActionOperation::CreateMgIntent{};
    if (kind == OperationKind::Behavior && name == "CreateMGTimeIntentFromAGIntent")
        return ActionOperation::CreateMgTimeIntent{};
    if (kind == OperationKind::Behavior && name == "CreateConstMGIntent")
        return ActionOperation::CreateConstMgIntent{};
    if (kind == OperationKind::Behavior && name == "BoardAdjust") return ActionOperation::BoardAdjust{};
    if (kind == OperationKind::Behavior && name == "JuiceHook") return ActionOperation::JuiceHook{};
    if (kind == OperationKind::Behavior && name == "BodyFlippingSignal") return ActionOperation::BodyFlippingSignal{};
    return ActionOperation::Unsupported{kind, std::string(name)};
}

s3::data::Result<std::optional<ActionInstance>> ActionFactory::create(OperationKind kind, Node /*parent*/,
                                                                      const Attributes& attributes) {
    const std::string* name = attributes.text("name");
    if (name == nullptr) return s3::data::Error{ActionFactoryError{"operation has no name attribute"}.to_string()};
    std::optional<ActionOperation> operation;
    if (kind == OperationKind::Condition) {
        auto extra = action_conditions::ActionCondition::parse(attributes);
        if (extra.is_err()) return s3::data::Error{extra.unwrap_err()};
        if (extra.unwrap()) {
            operation = ActionOperation::ExtraCondition{std::move(*extra.unwrap())};
        } else {
            auto condition = condition_nodes::parse(attributes);
            if (condition.is_err()) return s3::data::Error{condition.unwrap_err()};
            if (condition.unwrap()) {
                operation = ActionOperation::Condition{std::move(*condition.unwrap())};
            } else {
                operation = ActionFactory::operation(kind, *name);
            }
        }
    } else if (kind == OperationKind::Behavior && *name == "CreateTrickIntentFromGesture") {
        const std::string* group_text = attributes.text("group");
        auto group = s3::game::input::gesture_catalog::group_parse(group_text ? *group_text : std::string("Square"));
        if (group.is_err()) return s3::data::Error{group.unwrap_err()};
        operation = ActionOperation::CreateTrickIntentFromGesture{group.unwrap(), owned(attributes.text("override"))};
    } else if (kind == OperationKind::Behavior && *name == "PrintText2D") {
        const std::string* text = attributes.text("text");
        operation = ActionOperation::PrintText{text ? *text : std::string()};
    } else {
        operation = ActionFactory::operation(kind, *name);
    }
    return std::optional<ActionInstance>(
        ActionInstance{std::move(*operation), Parameter::from_attributes(attributes), {}});
}

s3::data::Status ActionFactory::add_parameter(ActionInstance& instance, const Attributes& attributes) {
    instance.parameters.push_back(Parameter::from_attributes(attributes));
    return s3::data::ok_status();
}

}  // namespace s3::game::graph_host::action_nodes
