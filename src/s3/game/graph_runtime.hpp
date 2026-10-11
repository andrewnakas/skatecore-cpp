// Port of skate-game/src/graph_runtime.rs.
//! Lossless bridge from the stock on-disk graph binding to the recovered TU3
//! dynamic-controller runtime.
//!
//! Concrete operations are intentionally retained by source operation ID. The
//! registration layer must construct every named operation before either graph
//! can execute; this module never substitutes a default handler.
#pragma once
#include <cstddef>
#include <string>
#include <variant>
#include <vector>

#include "s3/core/graph/activation.hpp"
#include "s3/core/graph/controller.hpp"
#include "s3/core/graph/selection.hpp"
#include "s3/core/rs.hpp"
#include "s3/data/manifest.hpp"
#include "s3/data/state_graph.hpp"
#include "s3/data/vfs.hpp"

namespace s3::game::graph_runtime {

using s3::data::GameAssets;
using s3::data::StateGraph;
using s3::data::state_graph_binding::Binding;
using s3::data::state_graph_binding::Node;
using s3::data::state_graph_binding::OperationKind;
using s3::graph::controller::Program;

/// Rust `enum GraphCompileError`; each variant is a nested struct held in `value`.
struct GraphCompileError {
    struct UnresolvedTransitionTarget {
        std::size_t transition = 0;
    };
    struct InvalidOperationParent {
        std::size_t operation = 0;
        const char* expected = "";  // &'static str
    };
    struct InvalidStateBehavior {
        std::size_t state = 0;
        std::size_t operation = 0;
    };
    struct InvalidTransitionHook {
        std::size_t transition = 0;
        std::size_t operation = 0;
    };
    struct InvalidExpressionChild {
        std::size_t expression = 0;
        Node node;
    };
    struct MissingConditionMask {
        std::size_t operation = 0;
    };

    std::variant<UnresolvedTransitionTarget, InvalidOperationParent, InvalidStateBehavior, InvalidTransitionHook,
                 InvalidExpressionChild, MissingConditionMask>
        value;

    GraphCompileError(UnresolvedTransitionTarget v) : value(v) {}
    GraphCompileError(InvalidOperationParent v) : value(v) {}
    GraphCompileError(InvalidStateBehavior v) : value(v) {}
    GraphCompileError(InvalidTransitionHook v) : value(v) {}
    GraphCompileError(InvalidExpressionChild v) : value(v) {}
    GraphCompileError(MissingConditionMask v) : value(v) {}

    /// `impl Display`.
    std::string to_string() const;
};
S3_DERIVE_EQ(GraphCompileError::UnresolvedTransitionTarget, transition)
[[maybe_unused]] inline bool operator==(const GraphCompileError::InvalidOperationParent& a,
                                        const GraphCompileError::InvalidOperationParent& b) {
    return a.operation == b.operation && std::string(a.expected) == b.expected;
}
[[maybe_unused]] inline bool operator!=(const GraphCompileError::InvalidOperationParent& a,
                                        const GraphCompileError::InvalidOperationParent& b) {
    return !(a == b);
}
S3_DERIVE_EQ(GraphCompileError::InvalidStateBehavior, state, operation)
S3_DERIVE_EQ(GraphCompileError::InvalidTransitionHook, transition, operation)
S3_DERIVE_EQ(GraphCompileError::InvalidExpressionChild, expression, node)
S3_DERIVE_EQ(GraphCompileError::MissingConditionMask, operation)
S3_DERIVE_EQ(GraphCompileError, value)

/// Rust `{:?}` of a binding `Node`, e.g. `Operation(3)`.
std::string debug_node(const Node& node);

/// Compact runtime IDs map back to the original binding operation IDs. The
/// future concrete host uses these tables to reach its exact factory instances.
struct OperationRemap {
    std::vector<std::size_t> behaviors;
    std::vector<std::size_t> conditions;
    std::vector<std::size_t> hooks;
};
S3_DERIVE_EQ(OperationRemap, behaviors, conditions, hooks)

struct CompiledGraph {
    Program program;
    OperationRemap operations;

    static rs::Result<CompiledGraph, GraphCompileError> from_binding(const Binding& binding);
};

struct LoadedGraph {
    StateGraph source;
    Binding binding;
    CompiledGraph runtime;
};

struct StockGraphs {
    LoadedGraph action;
    LoadedGraph motion;

    /// `root` is a filesystem directory (Rust `&Path`); the two graph files are read from disk.
    static rs::Result<StockGraphs, std::string> load(const std::string& root, const GameAssets& manifest);
    /// Same, reading the graph files from an existing Vfs (pack-backed hosts).
    static rs::Result<StockGraphs, std::string> load(const s3::data::Vfs& vfs, const std::string& root,
                                                     const GameAssets& manifest);
};

/// Private in Rust.
rs::Result<LoadedGraph, std::string> load_graph(const s3::data::Vfs& vfs, const std::string& root,
                                                const std::string& relative);

}  // namespace s3::game::graph_runtime
