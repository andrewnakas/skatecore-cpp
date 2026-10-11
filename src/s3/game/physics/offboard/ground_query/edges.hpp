// Port of skate-game/src/physics/offboard/ground_query/edges.rs
#pragma once
#include <vector>

#include "s3/game/physics/offboard/ground_query.hpp"

namespace s3::game::physics::offboard::ground_query::edges {

/// pub(super)
std::vector<Edge> candidates(const Scene& scene, const EdgeSearch& search);

}  // namespace s3::game::physics::offboard::ground_query::edges
