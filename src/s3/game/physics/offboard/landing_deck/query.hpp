// Port of skate-game/src/physics/offboard/landing_deck/query.rs
//! Real accelerated trajectory query and ordered completion of82D79948/79B40.
#pragma once
#include <string>

#include "s3/core/player/input_phase.hpp"
#include "s3/core/player/offboard/hippy_jump.hpp"
#include "s3/game/physics/offboard/landing_deck.hpp"

namespace s3::game::physics::offboard::landing_deck::query {

/// pub(super)
rs::Result<QueryResult, std::string> execute(const StaticScene& scene, native::QueryRequest request,
                                             const sc::player::input_phase::ProcessedPhysicsInput& processed);

}  // namespace s3::game::physics::offboard::landing_deck::query
