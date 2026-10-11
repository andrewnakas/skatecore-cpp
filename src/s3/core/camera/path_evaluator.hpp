// Port of skate-core/src/camera/path_evaluator.rs.
//! Complete TU3 82E023D0/82E02490 orchestration. Backend adapters are external;
//! the trajectory request and moving-sphere provider are distinct native objects.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

#include "s3/core/camera/_types.hpp"
#include "s3/core/camera/path_prediction.hpp"

namespace s3::camera {

/// Native request vtable slots +4/+8/+12 (82DF9270/82DF9338/82DF9340).
/// A nonzero flag selects the manager acceleration vector in the stock adapter.
struct TrajectoryCollisionRequest {
    virtual ~TrajectoryCollisionRequest() = default;
    virtual void submit(PredictionPath path, std::uint32_t context, std::uint8_t acceleration_flag) = 0;
    virtual bool is_ready() = 0;
    virtual float collision_time() = 0;
};

/// Native 82DF9400 provider. Implementations preserve native actor filtering and
/// sphere generation; the returned count must be at most the 50-record capacity.
struct MovingObstacleProvider {
    virtual ~MovingObstacleProvider() = default;
    virtual std::size_t collect(F4 position, F4 velocity, float radius, std::array<PathObstacle, 50>& output) = 0;
};

/// Fields touched by the two evaluator routines. The complete native constructor
/// and path setter own initialization; omitted native padding/fields are untouched.
struct PathEvaluator {
    /// Native word +8: only 0 and 1 have update behavior.
    std::uint32_t request_state = 0;
    PredictionPath path;
    /// Native byte +57, saved when submitting a request.
    std::uint8_t acceleration_flag = 0;
    float collision_time = 0.0f;
    float last_valid_time = 0.0f;
    /// Native byte +72. Readiness counts even for a negative/no-hit result.
    std::uint8_t found = 0;
    /// Native byte +73; only a ready asynchronous result replaces it.
    std::uint8_t result_acceleration_flag = 0;
    /// Native byte +76.
    std::uint8_t submitted_acceleration_flag = 0;
    /// Native word +80, compared as signed after wrapping increment.
    std::int32_t pending_polls = 0;

    /// Complete 82E02490. Does not clear current-frame result fields on entry.
    void update_request(std::uint32_t context, TrajectoryCollisionRequest& request);
    /// Complete 82E023D0, including the native asynchronous-before-moving order.
    void update(std::uint32_t context, TrajectoryCollisionRequest& request, MovingObstacleProvider& moving);
};
S3_DERIVE_EQ(PathEvaluator, request_state, path, acceleration_flag, collision_time, last_valid_time, found,
             result_acceleration_flag, submitted_acceleration_flag, pending_polls)

}  // namespace s3::camera
