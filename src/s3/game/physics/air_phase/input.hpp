// Port of skate-game/src/physics/air_phase/input.rs.
//! Actual Processed/animation/stock owners read by PhysicsAir and its launch.
#pragma once
#include <array>
#include <string>

#include "s3/core/air/state.hpp"
#include "s3/core/air/trajectory.hpp"
#include "s3/core/point_graph.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/fwd.hpp"

namespace s3::game::physics::air_phase::input {

using ::s3::air::state::PhysicsAirFrame;
using ::s3::air::state::PhysicsAirSettings;
using ::s3::air::trajectory::LaunchInfo;
using ::s3::air::trajectory::SelectorInput;

struct AirSettings {
    PhysicsAirSettings state{};
    float steering_blend = 0.0f;
    std::array<float, 5> grind_lock_distance{};

    static rs::Result<AirSettings, std::string> load(const sd::Collections& data);
};

PhysicsAirFrame frame(const SkaterRuntime& skater);
rs::Result<SelectorInput, std::string> selector_input(const GamePhysics& physics, const SkaterRuntime& skater);
rs::Result<LaunchInfo, std::string> launch_info(const GamePhysics& physics, const SkaterRuntime& skater);

}  // namespace s3::game::physics::air_phase::input
