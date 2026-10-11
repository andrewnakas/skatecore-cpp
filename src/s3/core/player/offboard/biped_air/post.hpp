// Port of skate-core/src/player/offboard/biped_air/post.rs (types).
// Air Post82D2FFF8 and its two approved offboard checks; canonical request storage.
// State::post_physics is defined in recovered.cpp.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/player/offboard/biped_air/post/checks.hpp"

namespace s3::player::offboard::biped_air::recovered::post {

using Vector = std::array<float, 4>;
using checks::Settings;

struct PostInput {
    std::uint32_t flags_2484 = 0;
    float state_timer_2664 = 0.0f;
    Vector forward_224{};
    Vector side_192{};
    float input_2708 = 0.0f;
    float input_2704 = 0.0f;
};

}  // namespace s3::player::offboard::biped_air::recovered::post
