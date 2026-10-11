// Port of skate-game/src/graph_runtime.rs.
#include "s3/game/graph_runtime.hpp"

#include <optional>

#include "s3/game/graph_runtime/disk.hpp"

namespace s3::game::graph_runtime {

using s3::graph::activation::ActivationProgram;
using s3::graph::activation::Child;
using s3::graph::activation::Condition;
using s3::graph::activation::Expression;
using s3::graph::controller::Behavior;
using s3::graph::selection::State;
using s3::graph::selection::Topology;
using s3::graph::selection::Transition;

std::string debug_node(const Node& node) {
    const char* kind = "State";
    switch (node.kind) {
        case Node::State: kind = "State"; break;
        case Node::Transition: kind = "Transition"; break;
        case Node::Expression: kind = "Expression"; break;
        case Node::Operation: kind = "Operation"; break;
    }
    return std::string(kind) + "(" + std::to_string(node.index) + ")";
}

std::string GraphCompileError::to_string() const {
    using std::to_string;
    if (const auto* e = std::get_if<UnresolvedTransitionTarget>(&value))
        return "stock graph transition " + to_string(e->transition) + " has no resolved target";
    if (const auto* e = std::get_if<InvalidOperationParent>(&value))
        return "stock graph operation " + to_string(e->operation) + " does not have its required " +
               std::string(e->expected) + " parent";
    if (const auto* e = std::get_if<InvalidStateBehavior>(&value))
        return "stock graph state " + to_string(e->state) + " references non-behavior operation " +
               to_string(e->operation);
    if (const auto* e = std::get_if<InvalidTransitionHook>(&value))
        return "stock graph transition " + to_string(e->transition) + " references non-hook operation " +
               to_string(e->operation);
    if (const auto* e = std::get_if<InvalidExpressionChild>(&value))
        return "stock graph expression " + to_string(e->expression) + " has invalid child " + debug_node(e->node);
    const auto& e = std::get<MissingConditionMask>(value);
    return "stock graph condition " + to_string(e.operation) + " has no parsed TU3 mask";
}

rs::Result<CompiledGraph, GraphCompileError> CompiledGraph::from_binding(const Binding& binding) {
    const std::size_t operation_count = binding.operations.size();
    std::vector<std::optional<std::size_t>> behavior_for_operation(operation_count);
    std::vector<std::optional<std::size_t>> condition_for_operation(operation_count);
    std::vector<std::optional<std::size_t>> hook_for_operation(operation_count);
    std::vector<std::size_t> behavior_operations;
    std::vector<std::size_t> condition_operations;
    std::vector<std::size_t> hook_operations;

    for (std::size_t operation_id = 0; operation_id < operation_count; ++operation_id) {
        switch (binding.operations[operation_id].kind) {
            case OperationKind::Behavior:
                behavior_for_operation[operation_id] = behavior_operations.size();
                behavior_operations.push_back(operation_id);
                break;
            case OperationKind::Condition:
                condition_for_operation[operation_id] = condition_operations.size();
                condition_operations.push_back(operation_id);
                break;
            case OperationKind::Hook:
                hook_for_operation[operation_id] = hook_operations.size();
                hook_operations.push_back(operation_id);
                break;
        }
    }

    Topology topology;
    topology.states.reserve(binding.states.size());
    for (const auto& state : binding.states) {
        State s;
        s.parent = state.parent;
        s.enabled = state.enabled != 0;
        s.active = state.active != 0;
        s.interruptibility = state.interruptibility;
        s.interrupt_ancestor = state.interrupt_ancestor;
        s.children = state.children;
        s.transitions = state.transitions;
        topology.states.push_back(std::move(s));
    }
    topology.transitions.reserve(binding.transitions.size());
    for (std::size_t id = 0; id < binding.transitions.size(); ++id) {
        const auto& transition = binding.transitions[id];
        if (!transition.target) {
            return rs::make_err(GraphCompileError(GraphCompileError::UnresolvedTransitionTarget{id}));
        }
        topology.transitions.push_back(Transition{transition.enabled != 0, *transition.target, transition.priority});
    }

    std::vector<Condition> conditions;
    conditions.reserve(condition_operations.size());
    for (std::size_t operation_id : condition_operations) {
        const auto& operation = binding.operations[operation_id];
        if (!operation.condition_mask) {
            return rs::make_err(GraphCompileError(GraphCompileError::MissingConditionMask{operation_id}));
        }
        conditions.push_back(Condition{operation.enabled != 0, *operation.condition_mask});
    }

    std::vector<Expression> expressions;
    expressions.reserve(binding.expressions.size());
    for (std::size_t expression_id = 0; expression_id < binding.expressions.size(); ++expression_id) {
        const auto& expression = binding.expressions[expression_id];
        std::vector<Child> children;
        children.reserve(expression.children.size());
        for (const Node& node : expression.children) {
            if (node.kind == Node::Expression) {
                children.push_back(Child(Child::Expression{node.index}));
            } else if (node.kind == Node::Operation && condition_for_operation[node.index]) {
                children.push_back(Child(Child::Condition{*condition_for_operation[node.index]}));
            } else {
                return rs::make_err(
                    GraphCompileError(GraphCompileError::InvalidExpressionChild{expression_id, node}));
            }
        }
        Expression e;
        e.operator_ = expression.op;
        e.children = std::move(children);
        expressions.push_back(std::move(e));
    }

    std::vector<Behavior> behaviors;
    behaviors.reserve(behavior_operations.size());
    for (std::size_t operation_id : behavior_operations) {
        const auto& operation = binding.operations[operation_id];
        if (operation.parent.kind != Node::State) {
            return rs::make_err(GraphCompileError(GraphCompileError::InvalidOperationParent{operation_id, "state"}));
        }
        behaviors.push_back(Behavior{operation.parent.index, operation.enabled != 0});
    }

    std::vector<std::vector<std::size_t>> state_behaviors;
    state_behaviors.reserve(binding.states.size());
    for (std::size_t state = 0; state < binding.states.size(); ++state) {
        std::vector<std::size_t> ids;
        for (std::size_t operation : binding.states[state].behaviors) {
            if (!behavior_for_operation[operation]) {
                return rs::make_err(GraphCompileError(GraphCompileError::InvalidStateBehavior{state, operation}));
            }
            ids.push_back(*behavior_for_operation[operation]);
        }
        state_behaviors.push_back(std::move(ids));
    }

    std::vector<std::vector<std::size_t>> transition_hooks;
    transition_hooks.reserve(binding.transitions.size());
    for (std::size_t transition = 0; transition < binding.transitions.size(); ++transition) {
        std::vector<std::size_t> ids;
        for (std::size_t operation : binding.transitions[transition].hooks) {
            if (!hook_for_operation[operation]) {
                return rs::make_err(
                    GraphCompileError(GraphCompileError::InvalidTransitionHook{transition, operation}));
            }
            ids.push_back(*hook_for_operation[operation]);
        }
        transition_hooks.push_back(std::move(ids));
    }

    CompiledGraph out;
    out.program.topology = std::move(topology);
    out.program.root = binding.root;
    ActivationProgram& activation = out.program.activation;
    for (const auto& state : binding.states) activation.state_expressions.push_back(state.expression);
    for (const auto& transition : binding.transitions)
        activation.transition_expressions.push_back(transition.expression);
    activation.expressions = std::move(expressions);
    activation.conditions = std::move(conditions);
    out.program.behaviors = std::move(behaviors);
    out.program.state_behaviors = std::move(state_behaviors);
    out.program.transition_hooks = std::move(transition_hooks);
    out.operations = OperationRemap{std::move(behavior_operations), std::move(condition_operations),
                                    std::move(hook_operations)};
    return out;
}

rs::Result<LoadedGraph, std::string> load_graph(const s3::data::Vfs& vfs, const std::string& root,
                                                const std::string& relative) {
    const std::string path = s3::data::join_path(root, relative);
    auto source = StateGraph::load(vfs, path);
    if (!source.ok) return rs::make_err(source.error);
    auto binding = Binding::from_graph(source.value);
    if (!binding.ok) return rs::make_err(binding.error);
    auto runtime = CompiledGraph::from_binding(binding.value);
    if (runtime.is_err()) {
        return rs::make_err("Invalid runtime graph " + path + ": " + runtime.unwrap_err().to_string());
    }
    return LoadedGraph{std::move(source.value), std::move(binding.value), std::move(runtime.unwrap())};
}

rs::Result<StockGraphs, std::string> StockGraphs::load(const s3::data::Vfs& vfs, const std::string& root,
                                                       const GameAssets& manifest) {
    auto action = load_graph(vfs, root, manifest.action_graph);
    if (action.is_err()) return rs::make_err(action.unwrap_err());
    auto motion = load_graph(vfs, root, manifest.motion_graph);
    if (motion.is_err()) return rs::make_err(motion.unwrap_err());
    return StockGraphs{std::move(action.unwrap()), std::move(motion.unwrap())};
}

rs::Result<StockGraphs, std::string> StockGraphs::load(const std::string& root, const GameAssets& manifest) {
    const s3::data::Vfs vfs = disk::read_files(root, {manifest.action_graph, manifest.motion_graph});
    return load(vfs, root, manifest);
}

}  // namespace s3::game::graph_runtime
