// Port of skate-core/src/animation/foot_ik/settings.rs.
// Authored IK parameters, resolved against the stock class layout.
#pragma once
#include <array>

#include "s3/core/animation/foot_ik/status.hpp"
#include "s3/core/animation/foot_ik/two_bone.hpp"

namespace s3::animation::foot_ik::settings {

using status::BlendSettings;
using two_bone::AngleLimits;

struct Settings {
    /// physics_animation/default layout876/880, consumed by82BF0FA8.
    AngleLimits angle_limits;
    BlendSettings blend;
    /// physics_skeletonik layout0/48/64/80.
    std::array<float, 4> post_ik_padding{};
    std::array<float, 4> contact_bounds{};
    std::array<float, 4> foot_on_deck_padding{};
    float wipeout_feet_offset = 0.0f;
    ///82BED418 retrieves physicsdeck hashes EE1FD008/1DCB828B/932190E7.
    float deck_half_width = 0.0f;
    float deck_half_length = 0.0f;
    float deck_total_half_length = 0.0f;
    /// physicsdeck.DeckFrontEndAngle, hashF9B732CA; retained authored degrees.
    float deck_front_angle_degrees = 0.0f;
};

}  // namespace s3::animation::foot_ik::settings
