// Port of skate-core/src/graph/activation.rs.
#include "s3/core/graph/activation.hpp"

#include "s3/core/graph/expression.hpp"

namespace s3::graph::activation {

bool ActivationProgram::node_activation(NodeId node, std::uint32_t mask, const Frame& frame,
                                        ConditionHost& host) const {
    std::optional<ExpressionId> expression;
    if (const auto* s = std::get_if<NodeId::State>(&node.value)) {
        expression = state_expressions[s->_0];
    } else {
        expression = transition_expressions[std::get<NodeId::Transition>(node.value)._0];
    }
    if (!expression) {
        return true;
    }
    std::uint8_t excluded = 1;
    const std::uint32_t result = expression_activation(*expression, mask, frame, excluded, host);
    return excluded != 0 || static_cast<std::uint8_t>(result) != 0;
}

std::uint32_t ActivationProgram::expression_activation(ExpressionId id, std::uint32_t mask, const Frame& frame,
                                                       std::uint8_t& excluded, ConditionHost& host) const {
    const Expression& expression = expressions[id];
    return expression::evaluate_operator(
        expression.operator_, expression.children.size(), excluded,
        [&](std::size_t index, std::uint8_t& child_excluded) -> std::uint32_t {
            const Child& child = expression.children[index];
            if (const auto* e = std::get_if<Child::Expression>(&child.value)) {
                return expression_activation(e->_0, mask, frame, child_excluded, host);
            }
            return condition_activation(std::get<Child::Condition>(child.value)._0, mask, frame, child_excluded,
                                        host);
        });
}

std::uint32_t ActivationProgram::condition_activation(ConditionId id, std::uint32_t mask, const Frame& frame,
                                                      std::uint8_t& excluded, ConditionHost& host) const {
    const Condition condition = conditions[id];
    if (!condition.enabled || (condition.mask & mask) == 0) {
        excluded = 1;
        return 1;
    }
    excluded = 0;
    return host.condition_activation(id, frame);
}

}  // namespace s3::graph::activation
