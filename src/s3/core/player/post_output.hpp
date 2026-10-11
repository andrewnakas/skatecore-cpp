// Port of skate-core/src/player/post_output.rs (whole file).
// TU3 `PhysicalPlayerHiLOD::PostOutput` (`0x82DB6CB0`).
//
// This is the final player-owned phase before the world finishes output and
// conditioners. The native body performs exactly two calls in this order.
#pragma once
#include "s3/core/rs.hpp"

namespace s3::player::post_output {

/// Opaque Player+1392-owned object passed to `0x82909510`.
///
/// Its internal layout is deliberately absent: PostOutput only passes its
/// address and the fixed native arguments `0` and `-1`.
struct PlayerOutputField1392 {};
S3_DERIVE_EQ_EMPTY(PlayerOutputField1392)

/// Required services called by the complete `0x82DB6CB0` wrapper.
struct PostOutputServices {
    virtual ~PostOutputServices() = default;
    /// `0x82909510(Player+1392, 0, -1)`.
    virtual void finish_player_output_82909510(PlayerOutputField1392& player_field_1392) = 0;
    /// `0x82BE2BA8(Player->Skeleton)`.
    virtual void finish_skeleton_output_82be2ba8() = 0;
};

/// Runs the complete TU3 PostOutput phase.
inline void run_post_output(PlayerOutputField1392& player_field_1392, PostOutputServices& services) {
    services.finish_player_output_82909510(player_field_1392);
    services.finish_skeleton_output_82be2ba8();
}

}  // namespace s3::player::post_output
