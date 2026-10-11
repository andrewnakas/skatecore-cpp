// Port of skate-game/src/graph_host/motion_grind/tests/routing.rs.
#include <string>
#include <utility>
#include <vector>

#include "harness.hpp"
#include "s3/game/graph_host/motion_conditions.hpp"
#include "s3/game/graph_host/motion_nodes.hpp"
#include "s3/game/graph_host/motion_stock_conditions.hpp"
#include "s3/game/graph_host/motion_stock_gameplay.hpp"

using namespace s3::game::graph_host;
using s3::data::GraphAttribute;
using s3::data::state_graph_attributes::Attributes;
using s3::data::state_graph_binding::Node;
using s3::data::state_graph_binding::OperationKind;

namespace {
/// `motion_grind::tests::attributes`.
std::vector<GraphAttribute> attributes(const std::vector<std::pair<const char*, const char*>>& pairs) {
    std::vector<GraphAttribute> out;
    for (const auto& [name, text] : pairs) out.push_back(GraphAttribute{name, text, 0, 0});
    return out;
}
}  // namespace

S3_TEST(production_factory_routes_three_handlers_and_all_five_conditions_to_grind_owner) {
    motion_nodes::MotionFactory factory;
    for (const char* name : {"CreateGrindAttributes", "ControlGrindCrouch", "GrindControlFade"}) {
        const auto a = attributes({
            {"name", name},
            {"distBoardToCogAnimAttribute", "height"},
            {"twistAnimAttribute", "twist"},
            {"twistMGIntent", "GrindBalanceX"},
        });
        auto created = factory.create(OperationKind::Behavior, Node{Node::State, 0}, Attributes(a));
        if (!created.ok) s3test::fail(__FILE__, __LINE__, created.error);
        CHECK(created.value.has_value() && created.value->is<motion_nodes::MotionOperation::Grind>());
        CHECK(!motion_stock_gameplay::Operation::recognizes(name));
    }
    for (const char* name : {"IsGrindBluntingBackslash", "IsGrindApproach", "GrindTrickOutTypeAllowed",
                             "IsLandingIntoGrind", "IsDroppingIn"}) {
        const auto a = attributes({{"name", name}, {"type", "right"}, {"Facing", "B"}});
        auto created = factory.create(OperationKind::Condition, Node{Node::State, 0}, Attributes(a));
        if (!created.ok) s3test::fail(__FILE__, __LINE__, created.error);
        const auto* condition =
            created.value ? std::get_if<motion_nodes::MotionOperation::Condition>(&created.value->value) : nullptr;
        CHECK(condition != nullptr && condition->_0.is<motion_conditions::MotionCondition::Grind>());
        CHECK(!motion_stock_conditions::Condition::recognizes(name));
    }
}

S3_TEST(production_factory_preserves_required_grind_attribute_errors) {
    motion_nodes::MotionFactory factory;
    for (const auto& [name, kind] : std::vector<std::pair<const char*, OperationKind>>{
             {"GrindControlFade", OperationKind::Behavior},
             {"GrindTrickOutTypeAllowed", OperationKind::Condition},
         }) {
        const auto a = attributes({{"name", name}});
        CHECK(!factory.create(kind, Node{Node::State, 0}, Attributes(a)).ok);
    }
}
