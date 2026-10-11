// Port of skate-game/src/graph_host/action.rs.
//! Typed ActionGraph registration and persistent operation instances.
//! The schedule and concrete gameplay sinks are owned by the caller.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "s3/core/animation/output/attributes.hpp"
#include "s3/core/graph/activation.hpp"
#include "s3/core/graph/conditions.hpp"
#include "s3/core/graph/controller.hpp"
#include "s3/core/graph/intent_handlers.hpp"
#include "s3/core/graph/intents.hpp"
#include "s3/core/input/body_flip_signal.hpp"
#include "s3/core/input/graph_intents.hpp"
#include "s3/core/rs.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/graph_host/action/board_adjust.hpp"
#include "s3/game/graph_host/action_nodes.hpp"
#include "s3/game/input/gesture_mapping.hpp"
#include "s3/game/graph_host/motion_gameplay_conditions.hpp"
#include "s3/game/graph_host/outputs.hpp"
#include "s3/game/graph_runtime.hpp"

namespace s3::game::graph_host::action {

using action_nodes::ActionFactory;
using action_nodes::ActionInstance;
using action_nodes::ActionInstances;
using action_nodes::ActionOperation;
using graph_runtime::LoadedGraph;
using graph_runtime::OperationRemap;
using outputs::ActionGraphInput;
using outputs::ActionGraphOutput;
using outputs::GraphCapabilityReport;
using outputs::GraphDiagnostics;
using s3::graph::conditions::ConditionInputs;
using s3::graph::controller::BehaviorId;
using s3::graph::controller::Frame;
using s3::graph::controller::HookId;
using s3::graph::intent_handlers::ConstMgIntent;
using s3::graph::intent_handlers::TimeMgIntent;
using s3::graph::intents::IntentMap;
using s3::input::graph_intents::CreateMgIntent;
using s3::input::graph_intents::IntentMutation;

/// Original State output consumed by82BA1680/82BA16F0.
struct PhysicalConditions {
    bool requests_dismount = false;
    std::uint32_t state = 0;
};

/// Runtime owner for the recovered ActionGraph intent path. Conditions read
/// current published physics and intent values when the graph evaluates them.
///
/// Graph advance entry point (as in Rust): `s3::graph::controller::Controller::update(program, dt,
/// host)` with this host and `graph.runtime.program`.
class ActionHost : public s3::graph::controller::Host {
public:
    ActionInstances instances;
    IntentMap action_intents;
    IntentMap motion_intents;
    ConditionInputs condition_inputs;
    std::optional<PhysicalConditions> physical_conditions;
    std::optional<motion_gameplay_conditions::GameplayConditions> gameplay_conditions;
    std::vector<s3::animation::output::attributes::AnimationAttribute> animation_attributes;
    std::optional<bool> is_tricking;
    std::optional<bool> dropping_in;
    std::optional<std::pair<bool, bool>> stance;
    // Private in Rust (kept accessible; no name clashes).
    std::uint64_t tick = 0;
    GraphDiagnostics errors;
    OperationRemap remap;
    std::vector<std::optional<std::size_t>> state_parents;
    std::uint32_t next_instance = 1;
    std::vector<bool> created;
    std::vector<ConstMgIntent> const_handlers;
    std::vector<TimeMgIntent> time_handlers;
    std::vector<board_adjust::State> board_adjust;
    std::vector<s3::input::body_flip_signal::State> body_flip;
    std::optional<s3::input::body_flip_signal::Settings> body_flip_settings;
    /// JuiceHook instance+8 starts with the native null intent name. Its
    /// update removes that pending key and resets it,82BA39A8.
    std::vector<std::string> juice_pending;
    std::vector<s3::game::input::gesture_mapping::State> trick_handlers;

    static rs::Result<ActionHost, std::string> from_graph(const LoadedGraph& graph,
                                                          const s3::data::Collections& data);
    static ActionHost new_(ActionInstances instances, OperationRemap remap);

    void apply(std::string name, IntentMutation mutation);
    void prepare_input(ActionGraphInput input);
    ActionGraphOutput output() const;

    /// Private in Rust.
    const ActionInstance* operation(BehaviorId id) const;
    void unsupported(BehaviorId id, const ActionInstance& instance);
    void publish_board_adjust(BehaviorId behavior, const ActionInstance& instance);

    // impl ConditionHost for ActionHost
    std::uint32_t condition_activation(std::size_t condition, const Frame& frame) override;
    // impl Host for ActionHost
    std::array<std::uint32_t, 6> context() const override { return {}; }
    std::uint32_t allocate(BehaviorId behavior, const Frame& frame) override;
    void begin(BehaviorId behavior, std::array<std::uint32_t, 6> context, const Frame& frame) override;
    void update(BehaviorId behavior, std::array<std::uint32_t, 6> context, const Frame& frame) override;
    void end(BehaviorId behavior, std::array<std::uint32_t, 6> context, const Frame& frame) override;
    void hook(HookId hook, const Frame& frame) override;
    void release(std::uint32_t /*instance*/) override {}
};

}  // namespace s3::game::graph_host::action
