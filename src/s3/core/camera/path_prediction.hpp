// Port of skate-core/src/camera/path_prediction.rs.
//! Complete candidate-list evaluator TU3 82E02608, used by avoidance82E023D0.
//! Query enumeration and its separate asynchronous world provider are upstream.
#pragma once
#include <cstddef>

#include "s3/core/camera/_types.hpp"

namespace s3::camera {

/// Fields consumed from each native 48-byte candidate; remaining words are not
/// interpreted by this evaluator. Velocity is in the same world frame as path.
struct PathObstacle {
    F4 position{};
    F4 velocity{};
    float radius = 0.0f;
};
S3_DERIVE_EQ(PathObstacle, position, velocity, radius)

struct PredictionPath {
    F4 position{};
    F4 velocity{};
    float radius = 0.0f;
    float horizon = 0.0f;
};
S3_DERIVE_EQ(PredictionPath, position, velocity, radius, horizon)

/// Returns the earliest accepted closest-approach time, or native FLT_MAX.
/// Native does not solve first surface entry time; tangent cases are rejected.
/// Rust `&[PathObstacle]` -> pointer + count.
float candidate_collision_time(PredictionPath path, const PathObstacle* obstacles, std::size_t count);

}  // namespace s3::camera
