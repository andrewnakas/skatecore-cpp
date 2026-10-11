// Port of skate-game/src/tests/graph_runtime.rs.
#include "harness.hpp"
#include "s3/game/graph_runtime.hpp"

using namespace s3::game::graph_runtime;
using s3::data::state_graph_binding::Expression;
using s3::data::state_graph_binding::Operation;
using s3::data::state_graph_binding::State;
using s3::data::state_graph_binding::Transition;
using s3::graph::activation::Child;

static Binding binding() {
    Binding b;
    State state;
    state.element = 0;
    state.name = "root";
    state.parent = std::nullopt;
    state.children = {};
    state.behaviors = {0};
    state.transitions = {0};
    state.expression = 0;
    state.enabled = 1;
    state.active = 1;
    state.interruptibility = 2;
    state.interrupt_ancestor = 0;
    b.states = {state};
    Transition transition;
    transition.element = 1;
    transition.owner = 0;
    transition.target = 0;
    transition.enabled = 1;
    transition.priority = 3;
    transition.expression = 0;
    transition.hooks = {2};
    b.transitions = {transition};
    Expression expression;
    expression.element = 2;
    expression.enabled = 1;
    expression.op = 1;
    expression.children = {Node{Node::Operation, 1}};
    b.expressions = {expression};
    Operation behavior;
    behavior.element = 3;
    behavior.parent = Node{Node::State, 0};
    behavior.kind = OperationKind::Behavior;
    behavior.name = "behavior";
    behavior.enabled = 1;
    behavior.condition_mask = std::nullopt;
    behavior.parameters = {6};
    Operation condition;
    condition.element = 4;
    condition.parent = Node{Node::Expression, 0};
    condition.kind = OperationKind::Condition;
    condition.name = "condition";
    condition.enabled = 1;
    condition.condition_mask = 3;
    condition.parameters = {7, 8};
    Operation hook;
    hook.element = 5;
    hook.parent = Node{Node::Transition, 0};
    hook.kind = OperationKind::Hook;
    hook.name = "hook";
    hook.enabled = 1;
    hook.condition_mask = std::nullopt;
    hook.parameters = {};
    b.operations = {behavior, condition, hook};
    b.root = 0;
    return b;
}

S3_TEST(preserves_topology_and_authored_operation_identity) {
    const CompiledGraph compiled = CompiledGraph::from_binding(binding()).unwrap();

    CHECK_EQ(compiled.program.root, std::size_t(0));
    CHECK_EQ(compiled.program.topology.states[0].interruptibility, 2u);
    CHECK_EQ(compiled.program.topology.transitions[0].target, std::size_t(0));
    CHECK_EQ(compiled.program.topology.transitions[0].priority, 3u);
    CHECK(compiled.program.activation.state_expressions ==
          std::vector<std::optional<std::size_t>>{std::size_t(0)});
    CHECK(compiled.program.activation.transition_expressions ==
          std::vector<std::optional<std::size_t>>{std::size_t(0)});
    CHECK(compiled.program.activation.expressions[0].children == std::vector<Child>{Child(Child::Condition{0})});
    CHECK_EQ(compiled.program.activation.conditions[0].mask, 3u);
    CHECK_EQ(compiled.program.behaviors[0].owner, std::size_t(0));
    CHECK(compiled.program.state_behaviors == std::vector<std::vector<std::size_t>>{{0}});
    CHECK(compiled.program.transition_hooks == std::vector<std::vector<std::size_t>>{{0}});
    CHECK(compiled.operations.behaviors == std::vector<std::size_t>{0});
    CHECK(compiled.operations.conditions == std::vector<std::size_t>{1});
    CHECK(compiled.operations.hooks == std::vector<std::size_t>{2});
}

S3_TEST(refuses_an_unresolved_transition_instead_of_choosing_a_fallback) {
    Binding source = binding();
    source.transitions[0].target = std::nullopt;

    CHECK(CompiledGraph::from_binding(source).unwrap_err() ==
          GraphCompileError(GraphCompileError::UnresolvedTransitionTarget{0}));
}
