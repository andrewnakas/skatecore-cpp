// Port of skate-core/src/animation/foot_ik/mod.rs.
// Native skeleton IK, TU3 GeneralUpdate82BDCA38 and its six update stages.
#pragma once
#include "s3/core/animation/foot_ik/blend.hpp"
#include "s3/core/animation/foot_ik/contact.hpp"
#include "s3/core/animation/foot_ik/drive.hpp"
#include "s3/core/animation/foot_ik/external.hpp"
#include "s3/core/animation/foot_ik/math.hpp"
#include "s3/core/animation/foot_ik/physical_solve.hpp"
#include "s3/core/animation/foot_ik/post_contact.hpp"
#include "s3/core/animation/foot_ik/post_physics.hpp"
#include "s3/core/animation/foot_ik/settings.hpp"
#include "s3/core/animation/foot_ik/state.hpp"
#include "s3/core/animation/foot_ik/status.hpp"
#include "s3/core/animation/foot_ik/transforms.hpp"
#include "s3/core/animation/foot_ik/two_bone.hpp"

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
