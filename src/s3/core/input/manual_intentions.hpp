// Port of skate-core/src/input/manual_intentions.rs.
// TU3 Fill825999F0 -> manual producer8259BA28, after Derived input.
#pragma once
#include <cstdint>
#include <vector>

#include "s3/core/input/controller.hpp"
#include "s3/core/input/riding_intentions.hpp"

namespace s3::input::manual_intentions {

using controller::DerivedControllerInput;
using riding_intentions::RidingIntent;

/// Continuous Manual/ManualBrake belong to the input listener. The authored
/// graphs own engagement time, allowed states, balance and animation selection.
std::vector<RidingIntent> produce(const DerivedControllerInput& controller, std::uint32_t actor_flags);

}  // namespace s3::input::manual_intentions
