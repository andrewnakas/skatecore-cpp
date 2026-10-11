// Port of skate-game/src/graph_host/tests/motion_stock.rs (the `tests` module of motion.rs).
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>

#include "assets.hpp"
#include "harness.hpp"
#include "motion_host_fixture.hpp"
#include "s3/core/animation/playback_tree.hpp"
#include "s3/game/graph_host/motion/execution.hpp"

using namespace s3::game::graph_host::motion;
namespace fixture = s3test::motion_fixture;

namespace {

using s3::animation::playback_tree::PlaybackTree;

std::pair<std::size_t, std::size_t> count(const PlaybackTree& tree) {
    if (std::holds_alternative<PlaybackTree::Clip>(tree.value)) return {1, 0};
    auto fold = [](std::pair<std::size_t, std::size_t> init, const auto& children, auto&& get) {
        for (const auto& child : children) {
            const auto [c, p] = count(get(child));
            init.first += c;
            init.second += p;
        }
        return init;
    };
    auto self = [](const PlaybackTree& t) -> const PlaybackTree& { return t; };
    if (const auto* phase = std::get_if<PlaybackTree::PhaseBlend>(&tree.value)) {
        return fold({0, 1}, phase->_0.children, self);
    }
    if (const auto* blend = std::get_if<PlaybackTree::BlendSpace>(&tree.value)) {
        return fold({0, 0}, blend->_0.children, self);
    }
    if (std::holds_alternative<PlaybackTree::Transition>(tree.value)) {
        S3_PANIC("Authored bank unexpectedly contains a runtime transition");
    }
    if (const auto* bind = std::get_if<PlaybackTree::BindPose>(&tree.value)) return count(*bind->motion);
    const auto& selection = std::get<PlaybackTree::SelectionSpace>(tree.value);
    return fold({0, 0}, selection._0.candidates,
                [](const auto& candidate) -> const PlaybackTree& { return *candidate.tree; });
}

}  // namespace

// #[ignore = "requires the user's private stock graph, collection and animation artifacts"]
S3_TEST(stock_motion_host_loads_graph_settings_and_authored_riding_tree) {
    S3_REQUIRE_ASSETS(root);
    const auto graph = fixture::stock_graph(root);
    const MotionHost host = fixture::host(root, graph, fixture::context(std::nullopt, std::nullopt, std::nullopt));
    const PlaybackTree tree = host.animation.build_tree("BTREE_RIDING").unwrap();
    const auto [clips, phases] = count(tree);
    CHECK(clips > 1 && phases > 1);
    for (const char* name : {"B_MONGO_PUSH_CYC1", "B_MONGO_PUSH_CYC2", "B_MONGO_PUSH_INTO", "B_PUSH_CYC1",
                             "B_PUSH_CYC2", "B_PUSH_INTO", "B_PUSH_MONGO_OUT", "B_PUSH_OUT", "B_SWITCH"}) {
        const PlaybackTree push = host.animation.build_tree(name).unwrap();
        const auto [push_clips, push_phases] = count(push);
        CHECK_MSG(push_clips > 0 && push_phases > 0,
                  std::string("Authored push root ") + name + " lost its blend tree");
    }
    std::cout << "Production host: " << host.operations.size() << " operations, real riding closure: " << clips
              << " leaf instances, " << phases << " PhaseBlend instances\n";
}

/// Enumerate actual factory results; this is a support inventory, not a parity test.
// #[ignore = "requires the user's private stock MotionGraph"]
// The Rust test panics when SKATE3_MOTION_INVENTORY is unset; here that case is a SKIP so the
// default (asset-backed) run stays green. Set SKATE3_MOTION_INVENTORY to write the TSV.
S3_TEST(stock_motion_factory_support_inventory) {
    using s3::data::state_graph_binding::Node;
    using s3::data::state_graph_binding::OperationKind;
    S3_REQUIRE_ASSETS(root);
    const char* inventory = std::getenv("SKATE3_MOTION_INVENTORY");
    if (inventory == nullptr) {
        std::cout << "SKIP " << __func__ << " (no SKATE3_MOTION_INVENTORY)\n";
        return;
    }
    const auto graph = fixture::stock_graph(root);
    const auto& binding = graph.binding;
    MotionFactory factory;
    auto instantiated = binding.instantiate_operations<MotionOperation>(graph.source, factory);
    if (!instantiated.ok) s3test::fail(__FILE__, __LINE__, instantiated.error);
    std::string rows = "kind\tname\tfactory_status\toperation_id\truntime_id\tenabled\towner\tvariant\n";
    std::size_t counts[3] = {0, 0, 0};
    auto clean = [](std::string s) {
        for (char& c : s) {
            if (c == '\t' || c == '\r' || c == '\n') c = ' ';
        }
        return s;
    };
    for (std::size_t id = 0; id < binding.operations.size() && id < instantiated.value.operations.size(); ++id) {
        const auto& operation = binding.operations[id];
        const MotionOperation& implementation = instantiated.value.operations[id];
        const char* kind = "Behavior";
        std::size_t index = 0;
        switch (operation.kind) {
            case OperationKind::Behavior: kind = "Behavior"; index = 0; break;
            case OperationKind::Condition: kind = "Condition"; index = 1; break;
            case OperationKind::Hook: kind = "Hook"; index = 2; break;
        }
        const char* status = implementation.is<MotionOperation::Unsupported>() ? "Unsupported" : "Registered";
        std::string owner;
        if (operation.parent.kind == Node::State) {
            std::vector<std::string> names;
            std::optional<std::size_t> cursor = operation.parent.index;
            while (cursor) {
                names.push_back(binding.states[*cursor].name);
                cursor = binding.states[*cursor].parent;
            }
            for (auto it = names.rbegin(); it != names.rend(); ++it) {
                if (!owner.empty()) owner += ".";
                owner += *it;
            }
        } else {
            owner = s3::game::graph_runtime::debug_node(operation.parent);
        }
        rows += std::string(kind) + "\t" + clean(operation.name) + "\t" + status + "\t" + std::to_string(id) + "\t" +
                std::to_string(counts[index]) + "\t" + std::to_string(operation.enabled) + "\t" + clean(owner) +
                "\t" + clean(execution::debug_operation(implementation)) + "\n";
        counts[index] += 1;
    }
    CHECK_EQ(counts[0] + counts[1] + counts[2], binding.operations.size());
    std::ofstream(inventory, std::ios::binary) << rows;
    std::cout << "MotionGraph factory inventory: " << counts[0] << " behaviors, " << counts[1] << " conditions, "
              << counts[2] << " hooks; " << inventory << "\n";
}
