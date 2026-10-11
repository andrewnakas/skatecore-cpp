// Port of skate-core/src/camera/path_evaluator.rs.
#include "s3/core/camera/path_evaluator.hpp"

namespace s3::camera {
namespace {

// Preserve the native fsubs/fsel ordering (including unordered operands and ties).
float select_time(float current, float candidate) { return current - candidate >= 0.0f ? candidate : current; }

}  // namespace

void PathEvaluator::update_request(std::uint32_t context, TrajectoryCollisionRequest& request) {
    switch (this->request_state) {
        case 0:
            request.submit(this->path, context, this->acceleration_flag);
            this->submitted_acceleration_flag = this->acceleration_flag;
            this->request_state = 1;
            this->pending_polls = 0;
            break;
        case 1:
            if (request.is_ready()) {
                float result = request.collision_time();
                result = result < 0.0f ? rs::f32::MAX : result;
                this->found = 1;
                this->request_state = 0;
                this->result_acceleration_flag = this->submitted_acceleration_flag;
                this->collision_time = select_time(this->collision_time, result);
            } else {
                this->pending_polls = std::int32_t(std::uint32_t(this->pending_polls) + 1u);
                if (this->pending_polls > 3) {
                    this->request_state = 0;
                    this->last_valid_time = rs::f32::MAX;
                }
            }
            break;
        default:
            break;
    }
}

void PathEvaluator::update(std::uint32_t context, TrajectoryCollisionRequest& request,
                           MovingObstacleProvider& moving) {
    this->found = 0;
    this->collision_time = rs::f32::MAX;
    this->update_request(context, request);

    // Host scratch storage; only the provider's returned records are consumed.
    std::array<PathObstacle, 50> obstacles{};
    const std::size_t count = moving.collect(this->path.position, this->path.velocity, this->path.radius, obstacles);
    if (!(count <= obstacles.size())) {
        S3_PANIC("native moving-sphere capacity exceeded");
    }
    const float time = candidate_collision_time(this->path, obstacles.data(), count);
    if (time < this->path.horizon) {
        this->found = 1;
        this->collision_time = select_time(this->collision_time, time);
    }
    if (this->found != 0) {
        this->last_valid_time = this->collision_time;
    }
}

}  // namespace s3::camera
