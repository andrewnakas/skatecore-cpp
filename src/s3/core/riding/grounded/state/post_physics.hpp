// Port of skate-core/src/riding/grounded/state/post_physics.rs.
// `PhysState_PhysicsGround::UpdatePostPhysics` (`82D387A8`).
#pragma once
#include <cstdint>

#include "s3/core/rs.hpp"

namespace s3::riding::grounded::state::post_physics {

enum class GroundPostPhysicsStage {
    CheckForGroundWipeout,
    UpdateBoardPath,
};

template <class E>
struct GroundPostPhysicsError {
    GroundPostPhysicsStage stage = GroundPostPhysicsStage::CheckForGroundWipeout;
    E source;

    bool operator==(const GroundPostPhysicsError& o) const { return stage == o.stage && source == o.source; }
    bool operator!=(const GroundPostPhysicsError& o) const { return !(*this == o); }
};

/// Rust trait `GroundPostPhysicsServices` (duck-typed template parameter `S`).
/// Wipeout::CheckForGroundWipeout82D8F9E0 and board path82C05EC0.
/// There is deliberately no empty implementation.
///   using Error = ...;
///   rs::Result<rs::Unit, Error> check_for_ground_wipeout_82d8f9e0();
///   rs::Result<rs::Unit, Error> update_board_path_82c05ec0();

template <class S>
rs::Result<rs::Unit, GroundPostPhysicsError<typename S::Error>> update_post_physics(
    std::int32_t offboard_trajectory_state_1776, S& services) {
    using Error = GroundPostPhysicsError<typename S::Error>;
    {
        auto r = services.check_for_ground_wipeout_82d8f9e0();
        if (r.is_err()) return rs::make_err(Error{GroundPostPhysicsStage::CheckForGroundWipeout, r.unwrap_err()});
    }
    if (offboard_trajectory_state_1776 < 0) {
        auto r = services.update_board_path_82c05ec0();
        if (r.is_err()) return rs::make_err(Error{GroundPostPhysicsStage::UpdateBoardPath, r.unwrap_err()});
    }
    return rs::Unit{};
}

}  // namespace s3::riding::grounded::state::post_physics
