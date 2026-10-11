// Port of skate-core/src/player/wipeout_state/torque.rs (whole file).
// Original SkeletonState angular estimate82BEC0E8 and torque82BE3B58.
#pragma once
#include <array>

#include "s3/core/physics/skeleton_body.hpp"
#include "s3/core/player/wipeout_state/math.hpp"

namespace s3::player::wipeout_state::torque {

using math::V;

///4656 contains the same fractional masses used by physical COM publication:
///original Init82BEBAA8 divides each4560 mass by total5216 at82BEBB80.
V angular_velocity(const physics::skeleton_body::SkeletonPhysicalRecord& record, const std::array<float, 24>& fractional);
///82BE3B58 converts the control magnitude and direction into original
///per-part displacement accumulators. Actual solved velocities remain intact.
void apply(physics::skeleton_body::SkeletonBody& body, V physical_com, V control);

}  // namespace s3::player::wipeout_state::torque
