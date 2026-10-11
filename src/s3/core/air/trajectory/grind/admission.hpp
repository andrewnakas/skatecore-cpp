// Port of skate-core/src/air/trajectory/grind/admission.rs.
// Original S3 82D6A840 admission and82D6AF58 landing publication.
#pragma once
#include <optional>

#include "s3/core/air/trajectory/grind.hpp"

namespace s3::air::trajectory::grind::admission {

/// position/material stay intact; time, frame, trajectory and normal change.
void apply_admitted_target(Prediction& prediction, GrindTrajectoryCandidate candidate, Vector correction,
                           Vector support_normal, Vector reference_velocity, float maximum_adjust,
                           float landing_velocity_scalar, float landing_max_angle);

/// 82D6AF58: bias by cross-rail velocity, bound against the investigated
/// support normal, reject the rail axis, then normalize.
Vector landing_normal(Vector direction, Vector velocity, Vector support, float velocity_scalar,
                      float maximum_angle_degrees);

/// 82BD3E78, shared with the offboard surface-frame port. (private)
Vector clamp_angle(Vector target, Vector reference, float limit);

/// Admission and displacement portion82D6A840. The caller retries the next
/// ranked candidate on None, then applies82D609E0 and82D6AF58 on success.
/// This deliberately requires native surface evidence before any correction.
std::optional<Vector> admitted_displacement(Prediction prediction, GrindTrajectoryCandidate candidate, Primitive edge,
                                            Vector reference_velocity, Vector processed_592,
                                            GrindSurfaceEvidence surface, const GrindAssistLimits& limits,
                                            std::optional<float>& effective_lock_distance);

}  // namespace s3::air::trajectory::grind::admission

namespace s3::air::trajectory::grind {
// pub use admission::{admitted_displacement, apply_admitted_target};
using admission::admitted_displacement;
using admission::apply_admitted_target;
}  // namespace s3::air::trajectory::grind
