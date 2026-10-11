// Port of skate-game/src/physics/ground_animation.rs.
//! GroundAnimation103 lifecycle82D33BF8/33DB8/33C70/34150, original Skate3TU3.
#pragma once
#include <array>

#include "s3/core/air/ground_jump.hpp"
#include "s3/core/player/input_phase.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/fwd.hpp"
#include "s3/game/physics/ground_animation/board.hpp"
#include "s3/game/physics/ground_animation/settings.hpp"
#include "s3/game/physics/ground_animation/skeleton.hpp"

namespace s3::game::physics::ground_animation {

using settings::GroundAnimationSettings;

/// `#[derive(Default)]`
struct GroundAnimationRuntime {
    sc::air::ground_jump::GroundJump jump{};  //48..68
    bool launched = false;                    //80, reset at each update
    std::array<float, 4> launch_velocity{};   //96, selector launch-info2048

    ///82D34150 writes only its active fields after the common PhysOut reset.
    void fill(const sc::player::input_phase::ProcessedPhysicsInput& p,
              sc::player::input_phase::AirOutputFields& out) const;
};

Status enter(GamePhysics& physics, SkaterRuntime& skater);
Status advance(GamePhysics& physics, SkaterRuntime& skater);
Status exit(GamePhysics& physics, SkaterRuntime& skater);

}  // namespace s3::game::physics::ground_animation
