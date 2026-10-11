// Port of skate-core/src/physics/skeleton_landing_on_board.rs.
// Skeleton82BDDC68,82BE0858,82BE0CC0 and82BDCC28, original S3 TU3.
// These calculate animation targets; the existing skeleton drives own physics.
#pragma once
#include <array>
#include <cstdint>
#include <optional>

#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/core/physics/skeleton_root.hpp"
#include "s3/core/point_graph.hpp"

namespace s3::physics::skeleton_landing_on_board {

using Frame = skeleton_animation_record::AnimationPartTransform;
using point_graph::PointGraph;
using skeleton_root::SkeletonRootFrames;

using Vector = std::array<float, 4>;

struct Settings {
    float root_y_offset = 0.0f;
    float capsule_radius = 0.0f;
    float capsule_length = 0.0f;
};

///82BDDC68 changes only skate-root and invokes COM-lift on the OLD animation root.
Frame skate_root(Frame old, Vector physical_com, std::uint32_t flags2480, float ground_y, Settings settings);

///82BE0858. Packet trajectory reset is performed by the owning host before this.
void update_root(SkeletonRootFrames& roots, Vector com, Vector local_com, float spin,
                 std::optional<std::uint32_t> revert_frames, bool reversed);

///82BDCC28 returns no write outside its descending/unsuppressed branch.
std::optional<Frame> pose_adjustment(const Frame& world_to_animation, const Frame& actual_board,
                                     const Frame& mapped_board, Vector ik_offset, float com_velocity_y,
                                     std::uint32_t flags2480, float time, const PointGraph<8>& blend);

}  // namespace s3::physics::skeleton_landing_on_board
