// Port of skate-core/src/air/known/post_physics.rs.
// TU3 KnownAir post-physics and upside-down wipeout handling.
#pragma once
#include "s3/core/air/known/data.hpp"
#include "s3/core/air/known/runtime.hpp"

namespace s3::air::known::post_physics {

using data::KnownAirFrame;
using data::KnownAirModeSettings;
using data::KnownAirState;
using data::KnownAirWipeoutRequest;
using data::KnownAirWipeoutSettings;
using runtime::KnownAirRuntime;

/// `PhysState_KnownAir::UpdatePostPhysics`, TU3 `0x82D36590`.
void update_post_physics(KnownAirState& state, const KnownAirFrame& frame, const KnownAirModeSettings& mode,
                         const KnownAirWipeoutSettings& settings, KnownAirWipeoutRequest& request,
                         KnownAirRuntime& runtime);

/// `PhysState_KnownAir::CheckUpsideDownFallingWipeout`, TU3 `0x82D36650`.
void check_upside_down_falling_wipeout(const KnownAirState& state, const KnownAirFrame& frame,
                                       const KnownAirModeSettings& mode, const KnownAirWipeoutSettings& settings,
                                       KnownAirWipeoutRequest& request, KnownAirRuntime& runtime);

}  // namespace s3::air::known::post_physics
