// Port of skate-game/src/graph_host/condition_nodes.rs.
//! Stock condition attributes bound to core evaluations. Values use the binary
//! float/bool encodings directly, including constructor precedence and defaults.
#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "s3/core/animation/output/attributes.hpp"
#include "s3/core/animation/slice.hpp"
#include "s3/core/graph/conditions.hpp"
#include "s3/core/graph/controller.hpp"
#include "s3/core/rs.hpp"
#include "s3/data/state_graph.hpp"
#include "s3/game/graph_runtime.hpp"

namespace s3::game::graph_host::action_nodes {
struct ActionInstances;
}

namespace s3::game::graph_host::condition_nodes {

using s3::data::state_graph_attributes::Attributes;
using s3::graph::conditions::ActionCondition;
using s3::graph::conditions::Comparison;
using s3::graph::conditions::NumericCondition;

///Original82BA3A78: first cached record, sequence filter, then numeric scalar
///comparison only for kinds0/2. Shared by the ActionGraph and MotionGraph.
bool animation_attribute(s3::animation::Slice<const s3::animation::output::attributes::AnimationAttribute> attributes,
                         s3::animation::output::attributes::AttributeName name, std::int32_t sequence_id,
                         NumericCondition numeric);
///Original82BA45D0/82BA4658 rejects an inactive containing state's time.
bool state_time(const s3::graph::controller::Frame& frame, std::optional<std::size_t> target,
                NumericCondition numeric);
/// `pub(super)` in Rust.
rs::Result<std::optional<ActionCondition>, std::string> parse(const Attributes& attributes);
NumericCondition numeric(const Attributes& attributes);
/// `pub(super)` in Rust.
void bind_current_states(const graph_runtime::LoadedGraph& graph, action_nodes::ActionInstances& instances);

}  // namespace s3::game::graph_host::condition_nodes
