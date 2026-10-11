// Port of skate-game/src/physics/biped_ground/services.rs
//! Concrete, stateless Ground bindings to the single SkaterRuntime owners.
//! Grab-spline queries/actions are a separate missing host, not board possession.
#pragma once
#include <optional>
#include <string>

#include "s3/core/player/offboard/air_launch.hpp"
#include "s3/core/player/offboard/biped_air/feet.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/biped_ground/sync.hpp"
#include "s3/game/physics/fwd.hpp"

namespace s3::game::physics::biped_ground::services {

namespace air_launch = ::s3::player::offboard::air_launch;
namespace feet = ::s3::player::offboard::biped_air::recovered::feet;
using sync::LaunchInput;

///82D770D8; S2 OffBoardFeetIK::ConsiderReset82DCD110 corroborates the owner.
///Do not reset targets/history merely because the board has been released.
void enter_feet(SkaterRuntime& skater);

///82D773F8 reads real line tests and82BE3220 world animation-record bones.
///Same input extraction as air_feet; neither its timer reset nor Air update
///is executed. Ground conditioning/history belong to super::feet::update.
feet::Input feet_input(const SkaterRuntime& skater);

void update_feet(SkaterRuntime& skater);

rs::Result<LaunchInput, std::string> launch_input(const SkaterRuntime& skater,
                                                   std::optional<air_launch::DepartureGeometry> geometry);

///82D32088..32108, after Skeleton has published its real processed fields.
void sync_grab(SkaterRuntime& skater);

///82D6CA58 copies the actual gravity vector and submits; consumption remains
///at the shared selector's native phase, not an invented Ground frame delay.
Status submit_air(GamePhysics& physics, SkaterRuntime& skater, air_launch::Packet packet);

}  // namespace s3::game::physics::biped_ground::services
