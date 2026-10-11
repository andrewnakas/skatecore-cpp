// Port of skate-game/src/physics/player_state/registry.rs.
//! Coordinator-owned physical state registry.
//!
//! TU3 has one lifecycle object per PhysicalPlayer state. Keeping the
//! support table here makes selection, transition, and diagnostics agree on
//! which owners are actually connected. Task-B-owned states remain explicit
//! unsupported entries until their native adapters are integrated.
#pragma once
#include "s3/core/player/state.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::player_state::registry {

using sc::player::state::PhysicalStateId;

/// `#[derive(Clone, Copy, Debug, PartialEq, Eq)]`
struct StateCapability {
    PhysicalStateId id = PhysicalStateId::Sleeping;
    bool supported = false;
    bool has_enter = false;
    bool has_exit = false;
};
S3_DERIVE_EQ(StateCapability, id, supported, has_enter, has_exit)

/// `#[derive(Clone, Copy, Debug, Default)]` unit struct.
struct StateRegistry {
    static constexpr StateRegistry new_() { return StateRegistry{}; }
    StateCapability capability(PhysicalStateId id) const;
    bool can_transition(PhysicalStateId current, PhysicalStateId requested) const;
};

}  // namespace s3::game::physics::player_state::registry
