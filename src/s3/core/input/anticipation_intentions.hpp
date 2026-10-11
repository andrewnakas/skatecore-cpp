// Port of skate-core/src/input/anticipation_intentions.rs.
// Right-stick anticipation publication, Fill825999F0:
// angle/magnitude 82599AD0..9C4C, emissions 8259AD0C..AD40.
#pragma once
#include <vector>

#include "s3/core/input/controller.hpp"
#include "s3/core/input/riding_intentions.hpp"

namespace s3::input::anticipation_intentions {

using controller::DerivedControllerInput;
using riding_intentions::RidingIntent;

std::vector<RidingIntent> produce(const DerivedControllerInput& controller);

}  // namespace s3::input::anticipation_intentions
