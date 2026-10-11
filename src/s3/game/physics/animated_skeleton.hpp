// Port of skate-game/src/physics/animated_skeleton.rs.
//! Stock skeleton-to-physics ownership. Animation globals and real physical
//! observations enter here; native calculations and history stay in core.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <tuple>

#include "s3/core/animation/slice.hpp"
#include "s3/core/physics/board_runtime.hpp"
#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/core/physics/skeleton_board_frames.hpp"
#include "s3/core/physics/skeleton_board_offset.hpp"
#include "s3/core/physics/skeleton_landing.hpp"
#include "s3/core/physics/skeleton_motion.hpp"
#include "s3/core/physics/skeleton_root.hpp"
#include "s3/core/point_graph.hpp"
#include "s3/data/animation_banks.hpp"
#include "s3/data/collections.hpp"
#include "s3/data/vfs.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::animated_skeleton {

using sc::physics::skeleton_animation_record::AnimationPartTransform;

/// `(AnimationPartTransform, [f32; 4], u32, f32)` landing-on-board pose inputs.
using LandingOnBoard = std::tuple<AnimationPartTransform, std::array<float, 4>, std::uint32_t, float>;

struct AnimatedSkeleton {
    sc::physics::skeleton_animation_record::SkeletonAnimationRecord record{};
    sc::physics::skeleton_root::SkeletonRootFrames roots{};
    sc::physics::skeleton_board_frames::SkeletonBoardFrames board_frames{};
    sc::physics::skeleton_board_offset::SkateboardOffset board_offset{};
    sc::physics::skeleton_landing::LandingAdjustment landing{};
    std::array<AnimationPartTransform, 4> targets{};
    sc::physics::skeleton_animation_record::SkeletonAnimationMasses masses{};
    std::array<std::size_t, 24> bone_indices{};
    std::array<AnimationPartTransform, 24> physics_frames{};
    AnimationPartTransform animation_board{};
    ///Skeleton12560, copied BEFORE board offsets and IK by82BD8CB0..8D10.
    AnimationPartTransform unadjusted_board{};
    // Private in Rust.
    sc::point_graph::PointGraph<8> landing_on_board_blend{};
    AnimationPartTransform animation_hips{};
    sc::physics::skeleton_motion::SkeletonMotion motion{};
    float board_at_y_delta = 0.0f;
    // Private in Rust.
    std::array<std::size_t, 4> target_bones{};
    sc::physics::skeleton_landing::LandingSettings landing_settings{};

    ///Existing stock82BED688 target mappings; left and right reparented hands.
    std::array<std::size_t, 2> reparented_hand_indices() const { return {target_bones[2], target_bones[3]}; }

    /// Rust `load(asset_root: &Path, ...)`.
    static rs::Result<AnimatedSkeleton, std::string> load(const sd::Vfs& vfs, const std::string& asset_root,
                                                          const sd::Collections& data,
                                                          const sd::AnimationFrames& animation, bool head_has_hat);

    /// ProcessData's mapped pose, target frames, landing adjustment, board offset,
    /// COM and final board cache. The caller supplies actual processed physical
    /// observations; this owner never substitutes the board body's COM for them.
    /// Grind/landing-on-board/offboard offset producers refresh board_offset
    /// before this call's landing stage when their native conditions require it.
    Status process_pose(sc::animation::Slice<const AnimationPartTransform> globals,
                        sc::physics::skeleton_landing::LandingInput landing, float dt, std::uint32_t& flags_2468,
                        std::uint32_t& flags_2472, std::optional<LandingOnBoard> landing_on_board);

    /// UpdateRootTransforms is called by the native ground/animated board update
    /// after ProcessData. Its result becomes the next ProcessData COM frame.
    void update_roots(const sc::physics::board_runtime::BoardRuntime& board, const AnimationPartTransform& reckoning,
                      float dt);

    ///Ground board phase before GeneralUpdate's IK and physical drives.
    AnimationPartTransform prepare_ground(const sc::physics::board_runtime::BoardRuntime& board,
                                          const AnimationPartTransform& reckoning, float dt,
                                          std::uint32_t& flags_2468);

    ///82BDF778..F7AC runs AFTER the current GeneralUpdate consumes trajectory.
    void finish_ground();
};

/// Private in Rust.
rs::Result<sc::physics::skeleton_landing::LandingSettings, std::string> load_landing_settings(
    const sd::Collections& data);

}  // namespace s3::game::physics::animated_skeleton
