// Port of skate-core/src/air/reckoning/mod.rs.
//
// PARTIAL PORT (stage C): only items used by physics (data.rs: AirState, Settings, Input).
// mod.rs (update/air states), math.rs and tests.rs are left to the air stage.
#pragma once
#include "s3/core/air/reckoning/data.hpp"

namespace s3::air::reckoning {
using data::AirState;
using data::Input;
using data::Settings;
}  // namespace s3::air::reckoning
