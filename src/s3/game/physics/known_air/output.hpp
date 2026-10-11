// Port of skate-game/src/physics/known_air/output.rs.
//! Fill82D36880 overwrites every field below except the explicitly retained pair.
#pragma once
#include "s3/core/air/known.hpp"
#include "s3/core/player/input_phase.hpp"

namespace s3::game::physics::known_air::output {

using ::s3::air::known::KnownAirOutput;
using ::s3::player::input_phase::AirOutputFields;

KnownAirOutput storage(const AirOutputFields& previous);
void publish(const KnownAirOutput& out, AirOutputFields& physical);

}  // namespace s3::game::physics::known_air::output
