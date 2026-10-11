// Port of skate-core/src/air/trajectory/grind/candidate.rs.
// Original S3 82D60C80/82D60B98/82D6A398/82D6A168.
#pragma once
#include <optional>
#include <vector>

#include "s3/core/air/trajectory/grind.hpp"

namespace s3::air::trajectory::grind::candidate {

/// 82C1F310 after provider traversal; pairwise rejection preserves global order.
std::vector<std::size_t> trajectory_box_filter(const std::vector<std::size_t>& indices,
                                               const std::vector<Primitive>& primitives, Vector takeoff);

/// 82D60C80's quadratic roots followed by82D60B98's later-root selection.
/// A double root must be strictly positive. The two-root branch takes max;
/// it does not add a positive-time restriction absent from the source.
std::optional<float> descending_plane_time(Trajectory t, Vector point, Vector normal);

/// 82D6A398. The padding is physics_trajectory+516, not a host snap radius.
std::optional<GrindTrajectoryCandidate> consider_grind_primitive(Prediction prediction, Primitive edge,
                                                                 std::size_t primitive, float padding);

/// 8296EC98's oriented angle then82E09C80's actual fractional-turn fold.
/// Keep the wrap arithmetic: acos(abs(dot)) is not binary32 equivalent. (private)
float folded_approach_angle(Vector a, Vector b, Vector normal);

/// 82D6A168 consumes one winner per call. Retrying after a rejected target
/// must retain the remaining insertion order, not sort the list by distance.
std::optional<GrindTrajectoryCandidate> take_best_grind(std::vector<GrindTrajectoryCandidate>& candidates,
                                                        float difficulty_distance,
                                                        const PointGraph<8>& height_penalty);

}  // namespace s3::air::trajectory::grind::candidate

namespace s3::air::trajectory::grind {
// pub use candidate::{consider_grind_primitive, descending_plane_time, take_best_grind, trajectory_box_filter};
using candidate::consider_grind_primitive;
using candidate::descending_plane_time;
using candidate::take_best_grind;
using candidate::trajectory_box_filter;
}  // namespace s3::air::trajectory::grind
