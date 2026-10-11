// Port of skate-core/src/physics/grind_forces/post.rs.
// Grind Post82D43098 -> Wipeout82D90898; S2 82D85E78 ->82DDF248.
// Unlike Ground, no cooldown/vehicle/squash/balance checks are added here.
#pragma once
#include "s3/core/physics/grind_forces.hpp"
#include "s3/core/player/wipeout.hpp"

namespace s3::physics::grind_forces::post {

/// Required physics_wipeout/default layout values. No borrowed Ground/Air
/// thresholds and no default-value fallback. Shared stock wiring supplies these.
struct Settings {
    float max_arm_contact_164 = 0.0f;
    float max_body_contact_168 = 0.0f;
    float xz_acceleration_204 = 0.0f;
    float max_displacement_208 = 0.0f;
    float max_angular_deck_error_212 = 0.0f;
};

void check(player::wipeout::Requests& state, Settings settings, const player::wipeout::Frame& frame);

}  // namespace s3::physics::grind_forces::post
