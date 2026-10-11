// Port of skate-core/src/graph/selection.rs.
//! Native dynamic-controller selection: 82C140D8 and its complete state-search
//! helpers. Topology and flags come from loaded graph objects, never a riding
//! state machine. The activation callback is Node::GetActivation's boundary.
#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include "s3/core/rs.hpp"

namespace s3::graph::selection {

using StateId = std::size_t;
using TransitionId = std::size_t;

/// Rust `enum NodeId { State(StateId), Transition(TransitionId) }`.
struct NodeId {
    struct State {
        StateId _0 = 0;
    };
    struct Transition {
        TransitionId _0 = 0;
    };
    std::variant<State, Transition> value;
    NodeId(State v) : value(v) {}
    NodeId(Transition v) : value(v) {}
};
S3_DERIVE_EQ(NodeId::State, _0)
S3_DERIVE_EQ(NodeId::Transition, _0)
S3_DERIVE_EQ(NodeId, value)

struct State {
    std::optional<StateId> parent;
    bool enabled = false;
    bool active = false;
    /// Native state+36: 0=false, 1=true, 2=resolved ancestor name.
    std::uint32_t interruptibility = 0;
    std::optional<StateId> interrupt_ancestor;
    std::vector<StateId> children;
    std::vector<TransitionId> transitions;
};

struct Transition {
    bool enabled = false;
    StateId target = 0;
    std::uint32_t priority = 0;
};

struct Topology {
    std::vector<State> states;
    std::vector<Transition> transitions;
};

/// `activate` callables are `FnMut(NodeId, u32) -> bool`.
struct Selection {
    const Topology& graph;
    StateId root;
    std::optional<StateId> current;

    /// Complete GetNextState 82C140D8. The output slot is explicit because an
    /// unrecognized interruptibility ordinal leaves it untouched in TU3.
    template <class F>
    std::optional<TransitionId> next(std::optional<StateId>& target, F&& activate) const {
        if (!current) {
            target = descend(root, 1, activate);
            return std::nullopt;
        }
        const StateId cur = *current;
        for (std::uint32_t priority = 4; priority-- > 0;) {
            if (auto found = search_transitions(cur, priority, activate)) {
                target = found->second;
                return found->first;
            }
        }
        // Complete retained/reselected-state path 82C14248.
        switch (graph.states[cur].interruptibility) {
            case 0:
                if (ancestors_active(cur, 2, activate)) {
                    target = cur;
                    break;
                }
                target = descend(root, 2, activate);
                break;
            case 1: target = descend(root, 2, activate); break;
            case 2: {
                const std::optional<StateId> ancestor_ = graph.states[cur].interrupt_ancestor;
                if (!ancestor_) S3_PANIC("native processed interrupt ancestor must resolve");
                const StateId ancestor = *ancestor_;
                if (ancestors_active(ancestor, 2, activate)) {
                    target = descend(ancestor, 2, activate);
                    if (target) {
                        return std::nullopt;
                    }
                }
                target = descend(root, 2, activate);
                break;
            }
            default: break;
        }
        return std::nullopt;
    }

    /// Complete 82C14710 (82C14708 is its thunk).
    template <class F>
    std::optional<StateId> descend(StateId state, std::uint32_t mask, F& activate) const {
        const std::uint32_t effective_mask = (mask == 2 && is_ancestor(state, current)) ? 2 : 1;
        if (!activate(NodeId(NodeId::State{state}), effective_mask)) {
            return std::nullopt;
        }
        return first_child_or_leaf(state, mask, activate);
    }

    template <class F>
    std::optional<StateId> first_child_or_leaf(StateId state, std::uint32_t mask, F& activate) const {
        const auto& children = graph.states[state].children;
        if (children.empty()) {
            return state;
        }
        for (StateId child : children) {
            const State& node = graph.states[child];
            if (node.enabled && node.active) {
                if (auto leaf = descend(child, mask, activate)) {
                    return leaf;
                }
            }
        }
        return std::nullopt;
    }

    /// Complete priority-specific SearchTransitions 82C14810, leaf to root.
    template <class F>
    std::optional<std::pair<TransitionId, StateId>> search_transitions(StateId state, std::uint32_t priority,
                                                                       F& activate) const {
        while (true) {
            for (TransitionId id : graph.states[state].transitions) {
                const Transition& transition = graph.transitions[id];
                const StateId target = transition.target;
                if (transition.priority != priority || !transition.enabled || !graph.states[target].enabled) {
                    continue;
                }
                if (!activate(NodeId(NodeId::Transition{id}), 1u) || !valid_target(current, target, activate) ||
                    !activate(NodeId(NodeId::State{target}), 1u)) {
                    continue;
                }
                if (auto leaf = first_child_or_leaf(target, 1, activate)) {
                    return std::make_pair(id, *leaf);
                }
            }
            const auto parent = graph.states[state].parent;
            if (!parent) return std::nullopt;
            state = *parent;
        }
    }

    /// Complete ValidTarget 82C14978. Target and new ancestors use preconditions;
    /// a shared ancestor (other than target) and its parents use mask 2.
    template <class F>
    bool valid_target(std::optional<StateId> current_, StateId target, F& activate) const {
        std::optional<StateId> next_ = target;
        while (next_) {
            const StateId state = *next_;
            if (state != target && is_ancestor(state, current_)) {
                return ancestors_active(state, 2, activate);
            }
            if (!activate(NodeId(NodeId::State{state}), 1u)) {
                return false;
            }
            next_ = graph.states[state].parent;
        }
        return true;
    }

    /// Complete state-parent activation chain 82C16B88 for State objects.
    template <class F>
    bool ancestors_active(StateId state, std::uint32_t mask, F& activate) const {
        while (true) {
            if (!activate(NodeId(NodeId::State{state}), mask)) {
                return false;
            }
            const auto parent = graph.states[state].parent;
            if (!parent) return true;
            state = *parent;
        }
    }

    bool is_ancestor(StateId ancestor, std::optional<StateId> state) const {
        while (state) {
            const StateId id = *state;
            if (id == ancestor) {
                return true;
            }
            state = graph.states[id].parent;
        }
        return false;
    }
};

}  // namespace s3::graph::selection
