// Port of skate-core/src/input/riding_intentions.rs.
// Push, brake, body-spin, steering, kick-turn, crouch, powerslide and world-grab intents
// from the TU3 ActionGraph input listener (`825999F0`).
#pragma once
#include <cstdint>
#include <ostream>
#include <string_view>
#include <vector>

#include "s3/core/input/controller.hpp"
#include "s3/core/rs.hpp"

namespace s3::input::riding_intentions {

using controller::DerivedControllerInput;

struct PushPreferences {
    /// Profile component+52, byte58. The host owns profile storage/selection.
    bool automatic_push_enabled = false;
    /// Profile component+56, byte156 selects the automatic push foot.
    bool automatic_push_right = false;
};

/// Rust `name: &'static str` -> std::string_view (compared by content).
struct RidingIntent {
    std::string_view name;
    float value = 0.0f;
};
S3_DERIVE_EQ(RidingIntent, name, value)
inline std::ostream& operator<<(std::ostream& os, const RidingIntent& i) {
    return os << "(\"" << i.name << "\", " << i.value << ")";
}

std::vector<RidingIntent> produce(const DerivedControllerInput& controller, std::uint32_t actor_flags,
                                  PushPreferences preferences);

}  // namespace s3::input::riding_intentions
