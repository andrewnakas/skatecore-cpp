// Port of skate-core/src/graph/activation.rs.
//! Data-driven activation for the stock state graph.
//!
//! Node::GetActivation 82C14C28 delegates to its expression with the caller's
//! mask. Condition::GetActivationMasked 82C12D48 excludes disabled conditions
//! and conditions whose authored mask does not intersect that mask. Expression
//! combination and its excluded-child rules are implemented in `expression`.
#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

#include "s3/core/graph/selection.hpp"
#include "s3/core/rs.hpp"

namespace s3::graph::controller {
struct Frame;
}

namespace s3::graph::activation {

using controller::Frame;
using selection::NodeId;

using ConditionId = std::size_t;
using ExpressionId = std::size_t;

/// Rust `enum Child { Expression(ExpressionId), Condition(ConditionId) }`.
struct Child {
    struct Expression {
        ExpressionId _0 = 0;
    };
    struct Condition {
        ConditionId _0 = 0;
    };
    std::variant<Expression, Condition> value;
    Child(Expression v) : value(v) {}
    Child(Condition v) : value(v) {}
};
S3_DERIVE_EQ(Child::Expression, _0)
S3_DERIVE_EQ(Child::Condition, _0)
S3_DERIVE_EQ(Child, value)

struct Expression {
    std::uint32_t operator_ = 0;
    /// Native child vector order. Evaluation is lazy, so this order is visible.
    std::vector<Child> children;
};

struct Condition {
    bool enabled = false;
    /// Parsed CondMask bits from the stock condition's `mask` attribute.
    std::uint32_t mask = 0;
};
S3_DERIVE_EQ(Condition, enabled, mask)

/// Concrete condition implementations are registered by the game. The graph
/// runtime decides whether they run; the host only evaluates an included leaf.
struct ConditionHost {
    virtual ~ConditionHost() = default;
    virtual std::uint32_t condition_activation(ConditionId condition, const Frame& frame) = 0;
};

struct ActivationProgram {
    std::vector<std::optional<ExpressionId>> state_expressions;
    std::vector<std::optional<ExpressionId>> transition_expressions;
    std::vector<Expression> expressions;
    std::vector<Condition> conditions;

    /// Complete Node::GetActivation boundary (82C14C28).
    bool node_activation(NodeId node, std::uint32_t mask, const Frame& frame, ConditionHost& host) const;

    std::uint32_t expression_activation(ExpressionId id, std::uint32_t mask, const Frame& frame,
                                        std::uint8_t& excluded, ConditionHost& host) const;

    /// Complete Condition::GetActivationMasked gate (82C12D48).
    std::uint32_t condition_activation(ConditionId id, std::uint32_t mask, const Frame& frame,
                                       std::uint8_t& excluded, ConditionHost& host) const;
};

}  // namespace s3::graph::activation
