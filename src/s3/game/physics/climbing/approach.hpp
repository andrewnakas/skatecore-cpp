// Port of skate-game/src/physics/climbing/approach.rs
//! Airborne reach is a presentation overlay. Stock graphs, gravity and collisions
//! retain ownership until both arm targets are physically within reach.
#pragma once
#include "s3/game/common.hpp"
#include "s3/game/physics/climbing/ledge.hpp"
#include "s3/game/physics/fwd.hpp"

namespace s3::game::physics::climbing {
struct Runtime;  // climbing.hpp
}

namespace s3::game::physics::climbing::approach {

struct Approach {
    ledge::Ledge ledge{};
    float weight = 0.0f;
};

void advance(const GamePhysics& physics, SkaterRuntime& skater, const PlayerControls& controls);
void update(Runtime& r, const GamePhysics& physics, SkaterRuntime& skater, const PlayerControls& controls);
float reach_gain(float distance, bool airborne);

}  // namespace s3::game::physics::climbing::approach
