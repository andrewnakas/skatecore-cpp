// Port of simref sim/src/camera/trajectory.rs.
//! Camera trajectory requests82DF9270/82DF9338/82DF9340. Rust executes the
//! world work synchronously; PathEvaluator still consumes it on its next poll.
#pragma once
#include <optional>
#include <string>

#include "s3/core/camera/path_evaluator.hpp"
#include "s3/core/physics/board_world.hpp"

namespace s3::game::camera::trajectory {

struct TrajectoryResult {
    bool ready = false;  // private in Rust
    float time = -1.0f;  // private in Rust
    std::optional<std::string> error;

    static TrajectoryResult new_() { return TrajectoryResult{false, -1.0f, std::nullopt}; }
};

struct CameraTrajectory : s3::camera::TrajectoryCollisionRequest {
    const s3::physics::board_world::BoardWorld* world = nullptr;
    s3::camera::F4 gravity{};
    TrajectoryResult* result = nullptr;

    CameraTrajectory(const s3::physics::board_world::BoardWorld& world_, s3::camera::F4 gravity_,
                     TrajectoryResult& result_)
        : world(&world_), gravity(gravity_), result(&result_) {}

    void submit(s3::camera::PredictionPath path, std::uint32_t context, std::uint8_t acceleration_flag) override;
    bool is_ready() override { return this->result->ready; }
    float collision_time() override { return this->result->time; }
};

}  // namespace s3::game::camera::trajectory
