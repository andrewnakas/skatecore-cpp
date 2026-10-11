// Port of skate-core/src/air/ground_jump/mod.rs.
// GroundAnimation's Toolkit_CalcGroundJump, original TU3 82D93618.
// Stock graph values and live animation/physical inputs determine launch.
#pragma once
#include "s3/core/air/ground_jump/types.hpp"

namespace s3::air::ground_jump {

// pub use types::{GroundJump, GroundJumpInput, GroundJumpMode, GroundJumpSettings};
using types::GroundJump;
using types::GroundJumpInput;
using types::GroundJumpMode;
using types::GroundJumpSettings;

GroundJump calculate(GroundJumpInput input, GroundJumpMode mode, const GroundJumpSettings& s);

}  // namespace s3::air::ground_jump
