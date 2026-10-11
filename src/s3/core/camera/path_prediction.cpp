// Port of skate-core/src/camera/path_prediction.rs.
#include "s3/core/camera/path_prediction.hpp"

#include "s3/core/camera/vector_tracker.hpp"

namespace s3::camera {

float candidate_collision_time(PredictionPath path, const PathObstacle* obstacles, std::size_t count) {
    float result = rs::f32::MAX;
    for (std::size_t k = 0; k < count; ++k) {
        const PathObstacle& obstacle = obstacles[k];
        const F4 separation = rs::from_fn<4>([&](std::size_t i) { return obstacle.position[i] - path.position[i]; });
        const F4 relative_velocity =
            rs::from_fn<4>([&](std::size_t i) { return obstacle.velocity[i] - path.velocity[i]; });
        const float radius = obstacle.radius + path.radius;
        const float clearance = vector_tracker::dot(separation, separation) - radius * radius;
        float time;
        if (clearance < 0.0f) {
            time = 0.0f;
        } else {
            const float approach = vector_tracker::dot(separation, relative_velocity);
            if (!(approach < 0.0f)) {
                continue;
            }
            const float speed_squared = vector_tracker::dot(relative_velocity, relative_velocity);
            const float t = (-1.0f / speed_squared) * approach;
            if (!(rs::mul_add(approach, t, clearance) < 0.0f)) {
                continue;
            }
            time = t;
        }
        if (time < path.horizon && time < result) {
            result = time;
        }
    }
    return result;
}

}  // namespace s3::camera
