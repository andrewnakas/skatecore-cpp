// Port of skate-core/src/physics/skeleton_board_frames.rs.
// Ground skeleton frame publication82BDF530 and COM lift82BDE310.
// The authored board11728, world target15952 and physical board12496 have
// different producers. Keep them separate through the animation/physics tick.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/core/physics/skeleton_root.hpp"

namespace s3::physics::skeleton_board_frames {

using Transform = skeleton_animation_record::AnimationPartTransform;
using skeleton_root::SkeletonRootFrames;

struct SkeletonBoardFrames {
    Transform physical_board = skeleton_animation_record::IDENTITY;
    Transform skate_root = skeleton_animation_record::IDENTITY;
    Transform animation_target = skeleton_animation_record::IDENTITY;
    Transform com_frame = skeleton_animation_record::IDENTITY;
    Transform lifted_com_frame = skeleton_animation_record::IDENTITY;
    std::array<float, 4> centre_of_mass{};
    std::array<float, 4> previous_centre_of_mass{};
    std::array<float, 4> com_velocity{};
    std::array<float, 4> local_centre_of_mass{};
    std::array<float, 4> local_board_position{};
    ///Skeleton16412, a per-update height limiter, not a timestep spring.
    float lift_height = 0.0f;

    static SkeletonBoardFrames default_() { return SkeletonBoardFrames{}; }

    ///Reset82BD9990: the retained COM seed is the supplied spawn position;
    ///the two COM drive frames initially share spawn plus one world-Y unit.
    ///The following physical observations replace the seed with weighted COM.
    void reset(Transform spawn);
    ///82BD9EA8, after the physical record observation: inverse-transform
    ///the retained physical COM and actual board position independently.
    void publish_local_observations(const SkeletonRootFrames& roots, const Transform& physical_board);
    ///82BD9FB0..82BDA060 after animation/physics errors are updated. This is
    ///physical weighted COM4464, not physical hips23 or authored hips height.
    void publish_centre_of_mass(std::array<float, 4> com, float dt, std::uint32_t flags_2472);
    ///82BDF55C..F774, following UpdateRootTransforms and preceding GeneralUpdate.
    ///Returns the complete frame published to ProcessedPhysIn0..48.
    Transform prepare_ground(const SkeletonRootFrames& roots, const Transform& mapped_board, Transform actual_board,
                             std::uint32_t& flags_2468);
    ///82BDF7C0. Teleport shares the authored target composition, but its COM
    ///drive frame starts one world-Y unit above the physical deck position.
    Transform prepare_teleport(const SkeletonRootFrames& roots, const Transform& mapped_board,
                               Transform actual_board, std::uint32_t& flags_2468);
    ///82BDE310. Preserve both fsel comparisons and the later vector FMA.
    void update_com_lift(const Transform& animation_to_world, std::array<float, 4> position, float requested_height);
};

}  // namespace s3::physics::skeleton_board_frames
