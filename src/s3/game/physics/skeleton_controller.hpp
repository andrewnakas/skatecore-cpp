// Port of skate-game/src/physics/skeleton_controller.rs.
//! Shared collision-policy controller used by Ground and both Air states.
//! Requests82D911A8 and ascending/falling update82D91660 precede the solve.
#pragma once
#include <cstdint>
#include <optional>
#include <string>

#include "s3/core/physics/skeleton_body.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::skeleton_controller {

struct SkeletonControllerState {
    std::uint32_t effective = 0;
    std::uint32_t requested = 0;
    bool has_request = false;
    bool override_enabled = false;
    ///Native byte18 tracks upward motion/jump input; it is not an enable bit.
    bool flag_18 = false;

    //Original inline constructor82DB2498..24B0.
    static SkeletonControllerState new_() { return SkeletonControllerState{}; }

    Status request_ground(sc::physics::skeleton_body::SkeletonCollisionMode& collision);

    Status request(std::uint32_t requested, sc::physics::skeleton_body::SkeletonCollisionMode& collision);

    ///Original request gate shared by driven and physical ragdoll adapters.
    std::optional<std::uint32_t> select_request(std::uint32_t requested);

    ///Original82D91660 changes the collision request only when byte18 changes.
    Status update_air(std::uint32_t processed_flags_2472, float velocity_y,
                      sc::physics::skeleton_body::SkeletonCollisionMode& collision);
};

}  // namespace s3::game::physics::skeleton_controller
