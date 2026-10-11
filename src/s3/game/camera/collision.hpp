// Port of simref sim/src/camera/collision.rs.
//! Host camera query adapter. The six source fat lines are preserved, and each
//! candidate uses the shared recovered TU3 thin/fat triangle dispatcher.
#pragma once
#include <array>
#include <optional>
#include <string>

#include "s3/core/camera/manager.hpp"
#include "s3/core/physics/board_world.hpp"

namespace s3::game::camera::collision {

struct CameraCollision : s3::camera::CollisionProvider {
    const s3::physics::board_world::BoardWorld* world = nullptr;
    std::array<s3::camera::FatLineResult, 6> results_{};  // private `results` in Rust
    std::optional<std::string> error;

    static CameraCollision new_(const s3::physics::board_world::BoardWorld& world);

    // impl DropCollisionProvider
    rs::Result<std::array<s3::camera::FatLineResult, 10>, std::string> query(
        std::array<s3::camera::FatLine, 10> lines, std::uint32_t context) override;
    // impl PositionerCollisionProvider
    void submit(std::array<s3::camera::FatLine, 6> lines, std::uint32_t context) override;
    std::array<s3::camera::FatLineResult, 6> results() override;
};

}  // namespace s3::game::camera::collision
