// Port of the #[cfg(test)] mod tests in skate-game/src/graph_host/motion_fakie_hold.rs
// (module `motion::fakie_hold`).
#include <cstddef>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "harness.hpp"
#include "s3/game/graph_host/motion/fakie_hold.hpp"

using namespace s3::game::graph_host::motion::fakie_hold;
using s3::data::GraphAttribute;
using s3::data::GraphElement;
using s3::data::StateGraph;
using s3::data::state_graph_binding::Binding;

namespace {
struct Tree {
    const char* tag;
    std::vector<std::pair<const char*, const char*>> attributes;
    std::vector<Tree> children;
};
Tree state(const char* name, std::vector<Tree> children) { return Tree{"state", {{"name", name}}, std::move(children)}; }
Tree condition() { return Tree{"condition", {{"name", "IsRidingFakie"}}, {}}; }
Tree transition(const char* target) {
    return Tree{"transition", {{"target", target}}, {Tree{"expression", {{"op", "and"}}, {condition()}}}};
}
std::size_t add(const Tree& tree, StateGraph& graph) {
    const std::size_t id = graph.elements.size();
    GraphElement element;
    element.source_offset = id;
    element.tag = tree.tag;
    for (const auto& [name, text] : tree.attributes) {
        GraphAttribute a;
        a.name = name;
        a.text = text;
        a.float_bits = 0;
        a.boolean_byte = 0;
        element.attributes.push_back(a);
    }
    graph.elements.push_back(element);
    std::vector<std::size_t> children;
    for (const Tree& child : tree.children) children.push_back(add(child, graph));
    graph.elements[id].children = children;
    return id;
}
}  // namespace

S3_TEST(gates_idle_and_bump_switch_but_preserves_other_fakie_conditions) {
    const Tree tree = state(
        "Motion",
        {state("OnBoard",
               {state("OnGround",
                      {
                          state("RidingIdle",
                                {
                                    state("Bumped", {transition("Riding.Turning.Switch")}),
                                    state("Riding", {state("Turning", {
                                                                          state("Idle", {transition("Switch")}),
                                                                          state("Switch", {}),
                                                                          state("Push", {Tree{"expression", {}, {condition()}}}),
                                                                      })}),
                                }),
                          // Same leaf name elsewhere must not be treated as automatic switching.
                          state("Other", {state("Idle", {transition("Switch")}), state("Switch", {})}),
                      })})});
    StateGraph graph;
    add(tree, graph);
    const auto binding = Binding::from_graph(graph);
    CHECK_MSG(binding.ok, binding.error);
    const std::vector<bool> gates = bind(binding.value);
    std::size_t on = 0, off = 0;
    for (bool gate : gates) (gate ? on : off) += 1;
    CHECK_EQ(on, std::size_t(2));
    CHECK_EQ(off, std::size_t(2));
    for (std::size_t id = 0; id < gates.size(); ++id) {
        CHECK_MSG(!blocked(false, gates, id), "disabled trainer must preserve stock transitions");
        CHECK_EQ(blocked(true, gates, id), static_cast<bool>(gates[id]));
    }
    CHECK(!blocked(true, gates, std::numeric_limits<std::size_t>::max()));
}
