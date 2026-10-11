// Port of skate-core/src/air/known/body.rs.
// Body flip and spin targets selected by TU3 KnownAir.
#pragma once
#include "s3/core/air/known/data.hpp"
#include "s3/core/air/known/runtime.hpp"

namespace s3::air::known::body {

using data::KnownAirFrame;
using data::KnownAirModeSettings;
using data::KnownAirReckoningFields;
using data::KnownAirSettings;
using data::KnownAirState;
using data::Vector4;
using runtime::KnownAirRuntime;

/// `PhysState_KnownAir::CalculateBodyFlipSpeed`, TU3 `0x82D35F40`.
float calculate_body_flip_speed(KnownAirState& state, const KnownAirFrame& frame, const KnownAirSettings& settings,
                                const KnownAirModeSettings& mode, const KnownAirReckoningFields& reckoning,
                                KnownAirRuntime& runtime);

/// `PhysState_KnownAir::CalculateBodySpinSpeed`, TU3 `0x82D360B0`.
float calculate_body_spin_speed(const KnownAirState& state, const KnownAirFrame& frame,
                                const KnownAirSettings& settings, const KnownAirModeSettings& mode,
                                const KnownAirReckoningFields& reckoning, KnownAirRuntime& runtime);

}  // namespace s3::air::known::body
