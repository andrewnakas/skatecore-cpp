// Port of simref sim/src/camera/trajectory.rs.
#include "s3/game/camera/trajectory.hpp"

#include "s3/core/camera/trajectory_query.hpp"
#include "s3/game/camera/world_query.hpp"

namespace s3::game::camera::trajectory {

using s3::camera::F4;

namespace {
rs::Result<std::optional<F4>, std::string> world_line(const s3::physics::board_world::BoardWorld& world, F4 start,
                                                      F4 end, float radius) {
    S3_CAM_TRY(auto hit, world_query::line(world, start, end, radius));
    if (hit.hit != 0) {
        return std::optional<F4>(hit.position);
    }
    return std::optional<F4>();
}
}  // namespace

void CameraTrajectory::submit(s3::camera::PredictionPath path, std::uint32_t /*context*/,
                              std::uint8_t acceleration_flag) {
    this->result->ready = false;
    this->result->error = std::nullopt;
    s3::camera::TrajectoryQuery query;
    query.position = path.position;
    query.velocity = path.velocity;
    query.gravity = acceleration_flag != 0 ? this->gravity : F4{};
    query.duration = path.horizon;
    query.radius = path.radius;
    // 82DF92BC/82DF9304 set both maximum-error fractions to1.
    query.start_error = 1.0f;
    query.end_error = 1.0f;
    const auto* world_ = this->world;
    auto time = query.collision_time<std::string>(
        [&](F4 start, F4 end, float radius) { return world_line(*world_, start, end, radius); });
    if (time.is_ok()) {
        this->result->time = time.unwrap();
        this->result->ready = true;
    } else {
        this->result->error = time.unwrap_err();
    }
}

}  // namespace s3::game::camera::trajectory
