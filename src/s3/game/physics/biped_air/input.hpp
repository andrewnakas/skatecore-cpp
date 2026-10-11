// Port of skate-game/src/physics/biped_air/input.rs
//! Direct processed/physical inputs for the distinct BipedAir lifecycle.
#pragma once
#include <string>

#include "s3/core/player/offboard/air_launch.hpp"
#include "s3/core/player/offboard/air_selector.hpp"
#include "s3/core/player/offboard/biped_air/height.hpp"
#include "s3/core/player/offboard/biped_air/recovered.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/fwd.hpp"

namespace s3::game::physics::biped_air::input {

using ::s3::player::offboard::air_launch::DepartureGeometry;
using ::s3::player::offboard::air_launch::Packet;
using ::s3::player::offboard::air_launch::Processed;
using ::s3::player::offboard::air_selector::Context;
using ::s3::player::offboard::biped_air::recovered::EnterInput;
using ::s3::player::offboard::biped_air::recovered::Frame;
using ::s3::player::offboard::biped_air::recovered::height::HeightInput;

Context context(const SkaterRuntime& skater);
Frame effective_frame(const SkaterRuntime& skater);
EnterInput enter(const SkaterRuntime& skater);
HeightInput height(const SkaterRuntime& skater);
rs::Result<Packet, std::string> launch_packet(const SkaterRuntime& skater);
Status submit(const GamePhysics& physics, SkaterRuntime& skater, Packet packet);

}  // namespace s3::game::physics::biped_air::input
