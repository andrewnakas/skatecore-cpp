// Port of skate-game/src/physics/ground_runtime/input.rs.
//! UpdateSkateboard82D38800 inputs from the actual processed packet, animation
//! attributes, physical pose and source board toolkit. No raw button-to-force
//! or guessed contact flags are introduced at this boundary.
//! (`impl GroundRuntime::prepare_toolkit` and `impl GroundSettings::input` are declared in their classes.)
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/math.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::ground_runtime::input {

/// Only values owned by separate state producers, not derivable from a rigid
/// body. The coordinator supplies the actual output of those producers.
struct GroundInputObservations {
    /// Reset82BF9EF0 writes0; preserve subsequent producer updates.
    float manual_drag_2724 = 0.0f;
    /// Actual trajectory output, Processed1776.
    std::uint32_t trajectory_state_bits = 0;
    /// Processed1516, copied from the grind/edge observation producer.
    std::uint32_t edge_flags = 0;
    /// Processed1232, the native detected edge point.
    std::array<float, 4> edge_point{};
};

/// Private helpers.
std::array<float, 4> raw(std::array<std::uint32_t, 4> v);
sc::math::Vector3 xyz(std::array<float, 4> v);
std::array<float, 4> v(sc::math::Vector3 v);

}  // namespace s3::game::physics::ground_runtime::input
