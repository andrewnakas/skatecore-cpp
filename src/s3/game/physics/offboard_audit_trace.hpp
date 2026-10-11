// Port of skate-game/src/physics/offboard_audit_trace.rs
//! TEMPORARY bottom-up audit observations; not native gameplay behavior.
//! Opt in with SKATE3_AUDIT_OFFBOARD_TICKS=start:end (inclusive simulation ticks).
//! Disabled by default. No gameplay state, guards, poses or forces are changed.
#pragma once
#include <cstdint>
#include <string_view>

#include "s3/game/common.hpp"
#include "s3/game/physics/fwd.hpp"

namespace s3::game::physics::offboard_audit_trace {

void stage(std::uint64_t tick, std::string_view phase, const GamePhysics& physics, const SkaterRuntime& s,
           const PlayerControls& controls);

}  // namespace s3::game::physics::offboard_audit_trace
