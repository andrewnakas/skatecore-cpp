// Port of skate-core/src/input/grind_intentions.rs.
// Original S3 Fill825999F0; emissions8259ACF0..8259AD94.
// Derived controller-space values: graph filters own stance/mirror transforms.
#pragma once
#include <vector>

#include "s3/core/input/controller.hpp"
#include "s3/core/input/riding_intentions.hpp"

namespace s3::input::grind_intentions {

using controller::DerivedControllerInput;
using riding_intentions::RidingIntent;

std::vector<RidingIntent> produce(const DerivedControllerInput& controller);

}  // namespace s3::input::grind_intentions
