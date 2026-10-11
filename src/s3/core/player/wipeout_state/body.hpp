// Port of skate-core/src/player/wipeout_state/body.rs (whole file).
// Original body adjustments used by WipeoutGround300.
#pragma once
#include "s3/core/physics/skeleton_body.hpp"
#include "s3/core/player/wipeout_state/math.hpp"

namespace s3::player::wipeout_state::body {

using math::V;
using physics::skeleton_body::SkeletonBody;

void set_velocity(SkeletonBody& body, V velocity);
void add_velocity(SkeletonBody& body, V velocity);
///82BE74C8. Upper parts retain a tenth of the normal component; lower parts zero it.
void remove_normal_velocity(SkeletonBody& body, V normal);
///82BE75B0 then82BD41B0: average with the requested velocity and limit distance2.
void blend_velocity(SkeletonBody& body, V target);
///82BE76B0: length and reciprocal each use the original two refinements.
void limit_velocity(SkeletonBody& body, float maximum);
///82D9CD60 and82D9CE58 publish per-second drag into the same live inertia.
void set_drag(SkeletonBody& body, float linear, float angular);
///82BE32D0.
void set_wipeout_drag(SkeletonBody& body, float drag);
///82BE3998. Source uses cached physical-record positions before the live writes.
void special_surface(SkeletonBody& body, float height);

}  // namespace s3::player::wipeout_state::body
