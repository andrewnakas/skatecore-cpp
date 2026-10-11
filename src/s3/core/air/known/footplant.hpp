// Port of skate-core/src/air/known/footplant.rs.
// Footplant branch embedded in TU3 KnownAir update.
#pragma once
#include "s3/core/air/known/data.hpp"
#include "s3/core/air/known/runtime.hpp"

namespace s3::air::known::footplant {

/// `0x82D36728`, preserving the reset gate and the three update-call order. pub(crate)
void update_footplant(const data::KnownAirState& state, const data::KnownAirFrame& frame,
                      runtime::KnownAirRuntime& runtime);

}  // namespace s3::air::known::footplant
