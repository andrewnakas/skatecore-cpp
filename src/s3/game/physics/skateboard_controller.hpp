// Port of skate-game/src/physics/skateboard_controller.rs.
//! Actual hand-controller lifecycle and board contact producer binding.
#pragma once
#include "s3/core/physics/board_ground.hpp"
#include "s3/core/physics/skateboard_controller.hpp"

namespace s3::game::physics::skateboard_controller {

using ::s3::physics::skateboard_controller::SkateboardController;

inline bool partial_request(const SkateboardController& controller,
                            const ::s3::physics::board_ground::BoardGroundState& ground) {
    //82C08450..8494 counts Board844..850 into byte868. This is not a
    //geometric overlap estimate or the separate four-wheel count869.
    return controller.request_partial_ragdoll(ground.part_contact_count);
}

}  // namespace s3::game::physics::skateboard_controller
