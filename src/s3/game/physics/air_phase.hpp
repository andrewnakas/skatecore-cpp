// Port of skate-game/src/physics/air_phase.rs.
//! PhysicsAir200 production lifecycle82D34388/82D346C0/82D346A0.
//! Uses the live player, board, skeleton and selector; no alternate air solver.
#pragma once
#include <array>

#include "s3/core/air/state.hpp"
#include "s3/core/rs.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/air_phase/board.hpp"
#include "s3/game/physics/air_phase/input.hpp"
#include "s3/game/physics/air_phase/skeleton.hpp"
#include "s3/game/physics/fwd.hpp"

namespace s3::game::physics::air_phase {

using input::AirSettings;
using input::launch_info;
using input::selector_input;

///Original initializer82F826B0/822F8B40. This COM trajectory acceleration is
///distinct from the world's separately supplied rigid-body gravity setting.
inline constexpr std::array<float, 4> COM_ACCELERATION = {0.0f, rs::from_bits(0xc11ccccdu), 0.0f, 0.0f};

Status enter(GamePhysics& physics, SkaterRuntime& skater);
void exit(SkaterRuntime& skater);
Status advance(GamePhysics& physics, SkaterRuntime& skater);
///82D34DD8. The common post-physics coordinator immediately follows this
///apex latch with Wipeout::CheckForAirWipeout(false), after collision feedback.
void update_apex(const GamePhysics& physics, ::s3::air::state::PhysicsAirState& state);

}  // namespace s3::game::physics::air_phase
