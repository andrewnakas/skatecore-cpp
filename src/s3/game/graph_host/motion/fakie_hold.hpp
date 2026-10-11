// Port of skate-game/src/graph_host/motion_fakie_hold.rs (module `motion::fakie_hold`).
//! Trainer gates for the stock automatic fakie-to-switch animation transitions.
//! FakieTurn is player steering and must never be suppressed by this option.
#pragma once
#include <cstddef>
#include <vector>

#include "s3/data/state_graph.hpp"

namespace s3::game::graph_host::motion::fakie_hold {

/// `pub(super)` in Rust.
std::vector<bool> bind(const s3::data::state_graph_binding::Binding& binding);
/// `pub(super)` in Rust.
inline bool blocked(bool enabled, const std::vector<bool>& gates, std::size_t operation) {
    return enabled && operation < gates.size() && gates[operation];
}

}  // namespace s3::game::graph_host::motion::fakie_hold
