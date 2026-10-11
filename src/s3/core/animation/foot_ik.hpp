// Port of skate-core/src/animation/foot_ik/mod.rs.
// Native skeleton IK, TU3 GeneralUpdate82BDCA38 and its six update stages.
//
// PARTIAL PORT (stage C): only items used by physics (math, transforms::inverse_rigid,
// drive::Geometry). blend, contact, external, physical_solve, post_contact, post_physics,
// settings, state, status and two_bone are left to the animation stage.
#pragma once
#include "s3/core/animation/foot_ik/math.hpp"
#include "s3/core/animation/foot_ik/transforms.hpp"
#include "s3/core/animation/foot_ik/drive.hpp"

namespace s3::animation::foot_ik {
// pub use math::interpolate as interpolate_native;
inline std::pair<math::Transform, float> interpolate_native(const math::Transform& a,
                                                            const math::Transform& b, float weight) {
    return math::interpolate(a, b, weight);
}
// pub use math::{interpolate_affine, inverse_affine};
using math::interpolate_affine;
using math::inverse_affine;
}  // namespace s3::animation::foot_ik
