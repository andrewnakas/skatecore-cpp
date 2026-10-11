// Port of the #[cfg(test)] module in skate-core/src/graph/activation.rs.
#include "harness.hpp"
#include "s3/core/graph/activation.hpp"
#include "s3/core/graph/controller.hpp"

using namespace s3::graph::activation;
using s3::graph::controller::Frame;

namespace {
struct Host : ConditionHost {
    std::vector<std::uint32_t> values;
    std::vector<ConditionId> calls;
    std::uint32_t condition_activation(ConditionId condition, const Frame&) override {
        calls.push_back(condition);
        return values[condition];
    }
};

Frame frame() { return Frame{0.0f, std::nullopt, std::nullopt, {}}; }

ActivationProgram program(std::uint32_t operator_, std::vector<Condition> conditions) {
    std::vector<Child> children;
    for (std::size_t i = 0; i < conditions.size(); ++i) children.push_back(Child::Condition{i});
    return ActivationProgram{{std::optional<ExpressionId>(0)}, {}, {Expression{operator_, children}}, conditions};
}
}  // namespace

S3_TEST(excluded_conditions_do_not_participate_and_all_excluded_is_true) {
    const auto graph = program(1, {Condition{false, 1}, Condition{true, 2}});
    Host host;
    host.values = {0, 0};
    CHECK(graph.node_activation(NodeId::State{0}, 1, frame(), host));
    CHECK(host.calls.empty());
}

S3_TEST(included_false_condition_controls_result_and_or_short_circuits) {
    const auto graph = program(2, {Condition{true, 1}, Condition{true, 1}});
    Host host;
    host.values = {1, 0};
    CHECK(graph.node_activation(NodeId::State{0}, 1, frame(), host));
    CHECK_EQ(host.calls, (std::vector<ConditionId>{0}));

    host.values[0] = 0;
    host.calls.clear();
    CHECK(!graph.node_activation(NodeId::State{0}, 1, frame(), host));
    CHECK_EQ(host.calls, (std::vector<ConditionId>{0, 1}));
}

S3_TEST(nodes_without_expressions_are_active) {
    const ActivationProgram graph{{std::nullopt}, {std::nullopt}, {}, {}};
    Host host;
    CHECK(graph.node_activation(NodeId::State{0}, 1, frame(), host));
    CHECK(graph.node_activation(NodeId::Transition{0}, 2, frame(), host));
}
