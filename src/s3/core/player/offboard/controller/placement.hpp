// Port of skate-core/src/player/offboard/controller/placement.rs.
// Original82D7B7A0, called with real GroundEnter frame/velocity/body position.
// State::place is defined in controller.cpp.
#pragma once
#include <cstdint>

#include "s3/core/player/offboard/controller/output.hpp"

namespace s3::player::offboard::controller {

struct PlacementInput {
    Frame frame{};           // entry0..48
    Vector velocity{};       // entry64
    Vector body_position{};  // entry80, actual skeleton body position
    std::uint32_t current_state = 0;   // processed2508
    std::uint32_t previous_state = 0;  // processed2504
    Frame previous_frame{};  // processed192..240
};

}  // namespace s3::player::offboard::controller
