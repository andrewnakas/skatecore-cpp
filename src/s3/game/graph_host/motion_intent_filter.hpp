// Port of skate-game/src/graph_host/motion_intent_filter.rs (module `motion_intent_filter`).
//! Stock FilterMotionGraphIntent constructor and map lifecycle adapter.
#pragma once
#include <string>
#include <utility>

#include "s3/core/animation/intent_filter.hpp"
#include "s3/core/graph/intents.hpp"
#include "s3/core/rs.hpp"
#include "s3/data/state_graph.hpp"

namespace s3::game::graph_host::motion_intent_filter {

using s3::animation::intent_filter::Settings;
using s3::animation::intent_filter::State;
using s3::data::state_graph_attributes::Attributes;
using s3::graph::intents::IntentMap;

struct Operation {
    std::string intent;
    std::string filtered_intent;
    Settings settings;

    /// TU3 82BB12C8. Presence flags remain distinct from numeric defaults.
    static Operation parse(const Attributes& a);

    void begin(State& state, IntentMap& output) const;
    void update(State& state, const IntentMap& input, IntentMap& output, float dt, std::pair<bool, bool> stance) const;
    /// End removes the key even when another active behavior wrote that alias.
    /// Native map removal 82BC1068 has no per-behavior ownership counter.
    void end(IntentMap& output) const;
};
S3_DERIVE_EQ(Operation, intent, filtered_intent, settings)

}  // namespace s3::game::graph_host::motion_intent_filter
