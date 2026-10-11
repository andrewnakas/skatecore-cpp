// Port of skate-core/src/graph/controller.rs.
//! Dynamic controller 82C13260, list preparation 82C13A00, Enter 82C13B20 and
//! Exit 82C13E58. The host owns concrete behavior/condition implementations and
//! instance storage. Graph topology is the processed stock asset.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "s3/core/animation/slice.hpp"
#include "s3/core/graph/activation.hpp"
#include "s3/core/graph/selection.hpp"
#include "s3/core/rs.hpp"

namespace s3::graph::controller {

using activation::ActivationProgram;
using activation::ConditionHost;
using selection::Selection;
using selection::StateId;
using selection::Topology;
using selection::TransitionId;

using BehaviorId = std::size_t;
using HookId = std::size_t;

struct Behavior {
    StateId owner = 0;
    bool enabled = false;
};

struct Program {
    Topology topology;
    StateId root = 0;
    ActivationProgram activation;
    std::vector<Behavior> behaviors;
    /// State+64 vector, in authored order.
    std::vector<std::vector<BehaviorId>> state_behaviors;
    /// Transition+44 vector, in authored order.
    std::vector<std::vector<HookId>> transition_hooks;
};

struct Frame {
    float dt = 0.0f;
    std::optional<StateId> current;
    std::optional<StateId> last;
    /// Missing entries correspond to absent native state-time map entries.
    std::vector<std::optional<float>> state_times;
};

struct ActiveBehavior {
    BehaviorId behavior = 0;
    /// Opaque host instance handle; zero is the native null instance.
    std::uint32_t instance = 0;
};
S3_DERIVE_EQ(ActiveBehavior, behavior, instance)

/// Concrete nodes run immediately. Effects of End, hooks and Begin are visible
/// to subsequent calls in this tick. A context is the native six-word value;
/// its pointer words are opaque handles at this platform-independent boundary.
/// The host resolves them and owns instance lifetime (including external refs).
struct Host : ConditionHost {
    virtual std::array<std::uint32_t, 6> context() const = 0;
    virtual std::uint32_t allocate(BehaviorId behavior, const Frame& frame) = 0;
    virtual void begin(BehaviorId behavior, std::array<std::uint32_t, 6> context, const Frame& frame) = 0;
    virtual void update(BehaviorId behavior, std::array<std::uint32_t, 6> context, const Frame& frame) = 0;
    virtual void end(BehaviorId behavior, std::array<std::uint32_t, 6> context, const Frame& frame) = 0;
    virtual void hook(HookId hook, const Frame& frame) = 0;
    /// Release the controller's instance ownership after its End callback.
    virtual void release(std::uint32_t instance) = 0;
};

struct Controller {
    Frame frame;
    std::vector<ActiveBehavior> active;

    static Controller new_(std::size_t state_count);

    void update(const Program& program, float dt, Host& host);

    /// DynamicHierarchicalController::EndAllBehaviours (82C13498). Native
    /// shutdown visits active behaviours in reverse order and does not alter
    /// the current state or state-time map; graph reset remains a separate act.
    void end_all_behaviors(Host& host);

    /// Complete 82C13A00 / 82C122E8 / 82C12260. Explicit transitions to a
    /// current ancestor re-enter that ancestor, including self transitions.
    std::pair<std::vector<StateId>, std::vector<StateId>> prepare_lists(const Program& program,
                                                                        std::optional<TransitionId> transition,
                                                                        std::optional<StateId> next);

    void exit(const Program& program, animation::Slice<const StateId> exiting, Host& host);

    void enter(const Program& program, animation::Slice<const StateId> entering, Host& host);
};

std::vector<StateId> path(const Topology& graph, std::optional<StateId> state);

}  // namespace s3::graph::controller
