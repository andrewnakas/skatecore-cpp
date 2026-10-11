// Port of skate-core/src/air/known/lifecycle.rs.
// TU3 KnownAir entry and exit lifecycle.
#pragma once
#include "s3/core/air/known/data.hpp"
#include "s3/core/air/known/runtime.hpp"

namespace s3::air::known::lifecycle {

using data::KnownAirFrame;
using data::KnownAirModeSettings;
using data::KnownAirReckoningFields;
using data::KnownAirSettings;
using data::KnownAirState;
using runtime::KnownAirRuntime;

/// `PhysState_KnownAir::Enter`, TU3 `0x82D352D0`.
void enter(KnownAirState& state, const KnownAirFrame& frame, const KnownAirSettings& settings,
           const KnownAirModeSettings& mode, KnownAirRuntime& runtime);

/// `PhysState_KnownAir::Exit`, TU3 `0x82D35920`.
void exit(KnownAirState& state, KnownAirFrame& frame, const KnownAirSettings& settings,
          KnownAirReckoningFields& reckoning, KnownAirRuntime& runtime);

}  // namespace s3::air::known::lifecycle
