// Port of skate-game/src/physics/landing_quality.rs.
//! Stock PhysicsAnimation cache +560/+576 and +592/+608, TU3 82DE6480.
#pragma once
#include <string>

#include "s3/core/animation/landing_quality.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::landing_quality {

rs::Result<sc::animation::landing_quality::Settings, std::string> load(const sd::Collections& data);

}  // namespace s3::game::physics::landing_quality
