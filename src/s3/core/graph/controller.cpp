// Port of skate-core/src/graph/controller.rs.
#include "s3/core/graph/controller.hpp"

#include <algorithm>

namespace s3::graph::controller {

Controller Controller::new_(std::size_t state_count) {
    Controller c;
    c.frame.dt = 0.0f;
    c.frame.state_times.assign(state_count, std::nullopt);
    return c;
}

void Controller::update(const Program& program, float dt, Host& host) {
    frame.dt = dt;
    std::optional<StateId> next;
    const std::optional<TransitionId> transition =
        Selection{program.topology, program.root, frame.current}.next(
            next, [&](selection::NodeId node, std::uint32_t mask) {
                return program.activation.node_activation(node, mask, frame, host);
            });
    auto lists = prepare_lists(program, transition, next);
    const std::vector<StateId>& exiting = lists.first;
    const std::vector<StateId>& entering = lists.second;

    for (auto& time : frame.state_times) {
        if (time) *time += dt;
    }
    exit(program, exiting, host);
    if (transition) {
        for (HookId hook : program.transition_hooks[*transition]) {
            host.hook(hook, frame);
        }
    }
    enter(program, entering, host);
    for (const ActiveBehavior& a : active) {
        auto context = host.context();
        context[2] = a.instance;
        host.update(a.behavior, context, frame);
    }
}

void Controller::end_all_behaviors(Host& host) {
    while (!active.empty()) {
        const ActiveBehavior a = active.back();
        auto context = host.context();
        context[2] = a.instance;
        host.end(a.behavior, context, frame);
        active.pop_back();
        host.release(a.instance);
    }
}

std::pair<std::vector<StateId>, std::vector<StateId>> Controller::prepare_lists(
    const Program& program, std::optional<TransitionId> transition, std::optional<StateId> next) {
    const std::vector<StateId> old_path = path(program.topology, frame.current);
    std::optional<StateId> boundary;
    if (!transition) {
        boundary = next;
    } else {
        const StateId target = program.topology.transitions[*transition].target;
        if (std::find(old_path.begin(), old_path.end(), target) != old_path.end()) {
            boundary = program.topology.states[target].parent;
        } else {
            boundary = target;
        }
    }
    const std::vector<StateId> boundary_path = path(program.topology, boundary);
    std::size_t common = 0;
    while (common < old_path.size() && common < boundary_path.size() &&
           old_path[common] == boundary_path[common]) {
        ++common;
    }
    std::vector<StateId> exiting(old_path.begin() + std::ptrdiff_t(common), old_path.end());
    const std::vector<StateId> next_path = path(program.topology, next);
    if (common > next_path.size()) S3_PANIC("slice index out of range");
    std::vector<StateId> entering(next_path.begin() + std::ptrdiff_t(common), next_path.end());
    frame.last = frame.current;
    frame.current = next;
    return {std::move(exiting), std::move(entering)};
}

void Controller::exit(const Program& program, animation::Slice<const StateId> exiting, Host& host) {
    for (std::size_t index = active.size(); index-- > 0;) {
        const ActiveBehavior a = active[index];
        const StateId owner = program.behaviors[a.behavior].owner;
        if (std::find(exiting.begin(), exiting.end(), owner) != exiting.end()) {
            auto context = host.context();
            context[2] = a.instance;
            host.end(a.behavior, context, frame);
            active.erase(active.begin() + std::ptrdiff_t(index));
            host.release(a.instance);
        }
    }
    // End observes the old state's final incremented time. Erasure follows
    // every End callback, before transition hooks and new state timers.
    for (StateId state : exiting) {
        frame.state_times[state] = std::nullopt;
    }
}

void Controller::enter(const Program& program, animation::Slice<const StateId> entering, Host& host) {
    // All new state timers exist before the first allocation or Begin.
    for (StateId state : entering) {
        frame.state_times[state] = 0.0f;
    }
    for (StateId state : entering) {
        for (BehaviorId behavior : program.state_behaviors[state]) {
            if (program.behaviors[behavior].enabled) {
                const std::uint32_t instance = host.allocate(behavior, frame);
                active.push_back(ActiveBehavior{behavior, instance});
                auto context = host.context();
                context[2] = instance;
                host.begin(behavior, context, frame);
            }
        }
    }
}

std::vector<StateId> path(const Topology& graph, std::optional<StateId> state) {
    std::vector<StateId> result;
    while (state) {
        const StateId id = *state;
        result.push_back(id);
        state = graph.states[id].parent;
    }
    std::reverse(result.begin(), result.end());
    return result;
}

}  // namespace s3::graph::controller
