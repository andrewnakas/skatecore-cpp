// Port of skate-game/src/graph_host/motion_fakie_hold.rs (module `motion::fakie_hold`).
#include "s3/game/graph_host/motion/fakie_hold.hpp"

#include <algorithm>
#include <optional>
#include <string>

namespace s3::game::graph_host::motion::fakie_hold {

using s3::data::state_graph_binding::Binding;
using s3::data::state_graph_binding::Node;

namespace {
void mark(const Binding& binding, std::size_t expression, std::vector<bool>& gates) {
    for (const Node& child : binding.expressions[expression].children) {
        if (child.kind == Node::Operation && binding.operations[child.index].name == "IsRidingFakie") {
            gates[child.index] = true;
        } else if (child.kind == Node::Expression) {
            mark(binding, child.index, gates);
        }
    }
}
}  // namespace

std::vector<bool> bind(const Binding& binding) {
    std::vector<bool> gates(binding.operations.size(), false);
    static const char* const expected[] = {"Motion", "OnBoard", "OnGround", "RidingIdle", "Riding", "Turning", "Switch"};
    for (const auto& transition : binding.transitions) {
        if (!transition.target) continue;
        std::vector<const std::string*> names;
        std::optional<std::size_t> state = transition.target;
        while (state) {
            names.push_back(&binding.states[*state].name);
            state = binding.states[*state].parent;
        }
        std::reverse(names.begin(), names.end());
        bool matches = names.size() == 7;
        for (std::size_t i = 0; matches && i < 7; ++i) matches = *names[i] == expected[i];
        if (!matches) continue;
        if (transition.expression) mark(binding, *transition.expression, gates);
    }
    return gates;
}

}  // namespace s3::game::graph_host::motion::fakie_hold
