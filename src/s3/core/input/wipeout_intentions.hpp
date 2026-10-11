// Port of skate-core/src/input/wipeout_intentions.rs.
// Wipeout intention helper8259BC28, called by Fill825999F0 at8259AA6C.
// Uses the existing evaluated stock action map and completed output flags.
#pragma once
#include <cstdint>
#include <vector>

#include "s3/core/input/controller.hpp"
#include "s3/core/input/riding_intentions.hpp"

namespace s3::input::wipeout_intentions {

using controller::DerivedControllerInput;
using riding_intentions::RidingIntent;

std::vector<RidingIntent> produce(const DerivedControllerInput& controller, std::uint32_t actor_flags_1908,
                                  std::uint32_t physical_flags_204);

}  // namespace s3::input::wipeout_intentions
