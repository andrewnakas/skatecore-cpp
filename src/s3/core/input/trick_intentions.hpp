// Port of skate-core/src/input/trick_intentions.rs.
// Player grab and aerial trick-control intents.
//
// This is the controller half of the TU3 `ActionGraphInputListener::Fill`
// path.  The listener publishes both grab variants every tick; the authored
// ActionGraph/MotionGraph decides which one is legal for the current stance.
#pragma once
#include <ostream>
#include <string_view>
#include <vector>

#include "s3/core/input/controller.hpp"
#include "s3/core/rs.hpp"

namespace s3::input::trick_intentions {

using controller::DerivedControllerInput;

struct TrickIntent {
    std::string_view name;
    float value = 0.0f;
};
S3_DERIVE_EQ(TrickIntent, name, value)
inline std::ostream& operator<<(std::ostream& os, const TrickIntent& i) {
    return os << "(\"" << i.name << "\", " << i.value << ")";
}

/// Recovered from `ActionGraphInputListener::Fill` (`825fb4c8`/`825999f0`):
/// ground grabs use exact trigger values, while air grabs use positive trigger
/// values.  Dark-catch requests come from the packed action flags, not from
/// gesture timing.
std::vector<TrickIntent> produce(const DerivedControllerInput& controller);

}  // namespace s3::input::trick_intentions
