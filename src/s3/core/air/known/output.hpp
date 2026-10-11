// Port of skate-core/src/air/known/output.rs.
// TU3 KnownAir PhysOut publication.
#pragma once
#include "s3/core/air/known/data.hpp"
#include "s3/core/air/known/runtime.hpp"

namespace s3::air::known::output {

/// `PhysState_KnownAir::FillPhysOut`, TU3 `0x82D36880`.
void fill_physics_output(const data::KnownAirState& state, const data::KnownAirFrame& frame,
                         data::KnownAirOutput& output, runtime::KnownAirRuntime& runtime);

}  // namespace s3::air::known::output
