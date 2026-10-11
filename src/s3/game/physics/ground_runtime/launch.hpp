// Port of skate-game/src/physics/ground_runtime/launch.rs.
//! Wall-jump launch packet;82D33448 constructs it,82BE33D0 fills it.
//! The native selector consumes this packet after body velocity publication.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/air/trajectory/types.hpp"
#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/core/riding/reckoning_frames.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::ground_runtime::launch {

using sc::physics::skeleton_animation_record::AnimationPartTransform;
using sc::riding::reckoning_frames::ReckoningFrames;

struct GroundLaunchPhysical {
    const ReckoningFrames& reckoning;
    ///Actual Skeleton16208/16240; do not substitute animation COM differences.
    std::array<float, 4> skeleton_vector_16208{};
    std::array<float, 4> skeleton_vector_16240{};
    ///Processed112,592,400,704,2468 and2604.
    std::array<float, 4> board_position{};
    std::array<float, 4> physical_center_of_mass{};
    std::array<float, 4> velocity{};
    std::array<float, 4> angular_velocity{};
    std::uint32_t flags_2468 = 0;
    float time_step = 0.0f;
};

/// `#[derive(Clone, Debug)]`; `impl Default` (member initialisers).
struct GroundLaunchInfo {
    AnimationPartTransform system = sc::physics::skeleton_animation_record::IDENTITY;
    AnimationPartTransform inverse_system = sc::physics::skeleton_animation_record::IDENTITY;
    std::array<float, 4> velocity{};
    std::array<float, 4> angular_velocity{};
    std::array<float, 4> skeleton_vector_16208{};
    std::array<float, 4> skeleton_vector_16240{};
    std::array<float, 4> board_position{};
    std::array<float, 4> physical_center_of_mass{};
    std::array<float, 4> vector_224{};
    std::array<float, 4> vector_240{};
    float cone_angle_x = 0.0f;
    float cone_angle_z = 0.0f;
    float time_step = 0.0f;
    bool flag_269 = false;
    bool wall_jump_ = false;  // Rust field `wall_jump` (renamed: clashes with method `wall_jump`)
    std::uint16_t flags_270 = 0;

    ///The retained Ground packet and the shared selector packet have the same
    ///82D33448 layout. Preserve both velocities and all override flags.
    sc::air::trajectory::types::LaunchInfo selector_launch() const;
    void fill(const GroundLaunchPhysical& p, float cone_angle_x, float cone_angle_z);
    ///Ground82D38CF4..D10 overrides both velocity records and byte268.
    void wall_jump(std::array<float, 4> velocity);
};

}  // namespace s3::game::physics::ground_runtime::launch
