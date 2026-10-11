// Port of skate-core/src/player/input_phase/motion_math.rs (whole file).
// Inlined physical input calculations from TU3 82DB4048.
// Xenon estimates use the shared, explicitly approximation-derived model.
#pragma once
#include "s3/core/player/input_phase/requests.hpp"

namespace s3::player::input_phase::motion_math {

using requests::GroundHistoryRequest;
using requests::GroundHistoryResult;
using requests::PrepareJumpRequest;
using types::RawVector;

///82DB5184..51D4: retain displacement separately from the damped velocity.
GroundHistoryResult ground_history(GroundHistoryRequest request);
///82DB53F0..5508 integrates the gravity component along the board velocity.
///The PrepareJump flag's direct velocity copy is handled by the caller.
RawVector prepared_jump_velocity(PrepareJumpRequest request);

}  // namespace s3::player::input_phase::motion_math
