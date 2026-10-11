// Port of skate-game/src/graph_host/condition_nodes.rs.
#include "s3/game/graph_host/condition_nodes.hpp"

#include <array>
#include <utility>
#include <vector>

#include "s3/game/graph_host/action_conditions.hpp"
#include "s3/game/graph_host/action_nodes.hpp"

namespace s3::game::graph_host::condition_nodes {

using s3::animation::output::attributes::AnimationAttribute;
using s3::animation::output::attributes::AttributeName;

bool animation_attribute(s3::animation::Slice<const AnimationAttribute> attributes, AttributeName name,
                         std::int32_t sequence_id, NumericCondition numeric) {
    for (const AnimationAttribute& a : attributes) {
        if (a.name == name) {
            return (sequence_id == -1 || sequence_id == a.sequence_id) &&
                   (numeric.comparison == Comparison::None || !(a.kind == 0 || a.kind == 2) ||
                    (a.payload._0[0] && numeric.matches(rs::from_bits(*a.payload._0[0]))));
        }
    }
    return false;
}

bool state_time(const s3::graph::controller::Frame& frame, std::optional<std::size_t> target,
                NumericCondition numeric) {
    if (!target || *target >= frame.state_times.size()) return false;
    const std::optional<float> time = frame.state_times[*target];
    return time && !(*time < 0.0f) && numeric.matches(*time);
}

rs::Result<std::optional<ActionCondition>, std::string> parse(const Attributes& attributes) {
    using C = ActionCondition;
    auto text_or = [&](const char* key) {
        const std::string* t = attributes.text(key);
        return t ? *t : std::string();
    };
    const std::string name = text_or("name");
    if (name == "HasAGIntent") return std::optional<C>(C::HasIntent{text_or("intent"), numeric(attributes)});
    if (name == "TimeSinceLastInput") return std::optional<C>(C::TimeSinceLastInput{numeric(attributes)});
    if (name == "PhysicsSpeedCompare") {
        // Rust struct-literal order: along_skate_z, then numeric.
        const bool along_skate_z = attributes.boolean_byte("alongSkateZ", 0) != 0;
        return std::optional<C>(C::Speed{along_skate_z, numeric(attributes)});
    }
    if (name == "PhysicsSpeedAndSlopeCompare") return std::optional<C>(C::SpeedAndSlope{numeric(attributes)});
    if (name == "PhysFilteredState") {
        const std::string state = text_or("state");
        static const std::array<std::pair<const char*, std::uint32_t>, 8> STATES = {{
            {"invalid", 0}, {"ground", 1}, {"air", 2}, {"grind", 3},
            {"wipeout", 4}, {"teleport", 5}, {"offboard", 6}, {"offboardair", 7},
        }};
        for (const auto& [text, id] : STATES) {
            if (state == text) return std::optional<C>(C::FilteredState{id});
        }
        return rs::make_err("PhysFilteredState has undefined state `" + state + "`");
    }
    if (name == "IsGrinding") {
        const std::string* grind = attributes.text("grindName");
        return std::optional<C>(C::Grinding{grind ? std::optional<std::string>(*grind) : std::nullopt});
    }
    if (name == "IsMirrored") return std::optional<C>(C::Mirrored{});
    if (name == "IsRidingFakie") return std::optional<C>(C::RidingFakie{});
    if (name == "DisablePushBrake") return std::optional<C>(C::DisablePushBrake{});
    if (name == "CurrentState") return std::optional<C>(C::CurrentState{text_or("state"), std::nullopt});
    return std::optional<C>();
}

NumericCondition numeric(const Attributes& attributes) {
    using C = Comparison;
    // TU382C126F0. The long spelling takes precedence over all short forms;
    // unrecognized long spellings retain the constructor's Equals default.
    //82AE89B0 confirms case-sensitive comparison.
    C comparison = C::None;
    const char* field = "";
    if (const std::string* value = attributes.text("comparison")) {
        if (*value == "NotEqual") {
            comparison = C::NotEqual;
        } else if (*value == "GreaterThan") {
            comparison = C::Greater;
        } else if (*value == "LessThan") {
            comparison = C::Less;
        } else if (*value == "GreaterEqual") {
            comparison = C::GreaterEqual;
        } else if (*value == "LessEqual") {
            comparison = C::LessEqual;
        } else {
            comparison = C::Equal;
        }
        field = "value";
    } else {
        static const std::array<std::pair<C, const char*>, 7> FIELDS = {{
            {C::Equal, "equal"},
            {C::NotEqual, "notEqual"},
            {C::Greater, "greater"},
            {C::GreaterAbsolute, "greaterThanAbs"},
            {C::Less, "less"},
            {C::GreaterEqual, "greaterEqual"},
            {C::LessEqual, "lessEqual"},
        }};
        for (const auto& [c, f] : FIELDS) {
            if (attributes.get(f) != nullptr) {
                comparison = c;
                field = f;
                break;
            }
        }
    }
    NumericCondition out;
    out.comparison = comparison;
    out.threshold = rs::from_bits(attributes.float_bits(field, 0));
    out.absolute = attributes.boolean_byte("abs", 0) != 0;
    return out;
}

void bind_current_states(const graph_runtime::LoadedGraph& graph, action_nodes::ActionInstances& instances) {
    std::vector<std::optional<std::size_t>> element_parents(graph.source.elements.size());
    for (std::size_t parent = 0; parent < graph.source.elements.size(); ++parent) {
        for (std::size_t child : graph.source.elements[parent].children) element_parents[child] = parent;
    }
    auto containing_state = [&](std::size_t operation) -> std::optional<std::size_t> {
        std::optional<std::size_t> element = element_parents[graph.binding.operations[operation].element];
        while (element) {
            const std::size_t id = *element;
            for (std::size_t s = 0; s < graph.binding.states.size(); ++s) {
                if (graph.binding.states[s].element == id) return s;
            }
            element = element_parents[id];
        }
        return std::nullopt;
    };
    for (std::size_t operation : graph.runtime.operations.conditions) {
        auto& op = instances.operations[operation].operation;
        if (auto* extra = std::get_if<action_nodes::ActionOperation::ExtraCondition>(&op.value)) {
            if (auto* parent_time =
                    std::get_if<action_conditions::ActionCondition::ParentTime>(&extra->_0.value)) {
                if (const auto state = containing_state(operation)) parent_time->target = *state;
                continue;
            }
        }
        auto* condition = std::get_if<action_nodes::ActionOperation::Condition>(&op.value);
        if (condition == nullptr) continue;
        auto* current = std::get_if<ActionCondition::CurrentState>(&condition->_0.value);
        if (current == nullptr) continue;
        // Node::GetParentState82C11B88 climbs through expressions/transitions
        // to the containing state; it is not the graph's current leaf.
        if (const auto state = containing_state(operation)) {
            current->target = graph.binding.find_state(*state, current->name, true);  //82C13728
        }
    }
}

}  // namespace s3::game::graph_host::condition_nodes
