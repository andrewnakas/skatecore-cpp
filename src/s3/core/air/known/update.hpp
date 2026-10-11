// Port of skate-core/src/air/known/update.rs (module air::known::update; C++ namespace `update_mod`
// because the re-exported fn `known::update` would clash with a same-named namespace).
// Full outer TU3 KnownAir update in recovered call order.
#pragma once
#include <array>

#include "s3/core/air/known/data.hpp"
#include "s3/core/air/known/runtime.hpp"

namespace s3::air::known::update_mod {

using data::GrindFootBody;
using data::KnownAirFrame;
using data::KnownAirModeSettings;
using data::KnownAirReckoningFields;
using data::KnownAirSettings;
using data::KnownAirState;
using runtime::KnownAirRuntime;

/// Module-private in Rust.
inline constexpr std::array<GrindFootBody, 6> GRIND_FOOT_BODIES = {
    GrindFootBody::Body3152, GrindFootBody::Body3156, GrindFootBody::Body3160,
    GrindFootBody::Body3168, GrindFootBody::Body3172, GrindFootBody::Body3176,
};

/// `PhysState_KnownAir::Update`, TU3 `0x82D35C58`.
void update(KnownAirState& state, const KnownAirFrame& frame, const KnownAirSettings& settings,
            const KnownAirModeSettings& mode, const KnownAirReckoningFields& reckoning, KnownAirRuntime& runtime);

}  // namespace s3::air::known::update_mod
