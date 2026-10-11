// Port of skate-game/src/physics/skeleton_air.rs.
//! Production animated-board and COM-controlled air Skeleton updates.
//! Original TU3 82BDDA10 / 82BDE600. All owners are the same objects used by
//! ProcessData, GeneralUpdate, the shared solve and final pose publication.
//
// `impl SkeletonInputRuntime { update_animated, update_known_air }` from this file is
// declared in skeleton_input_runtime.hpp and defined in skeleton_air.cpp.
#pragma once
#include <string>

#include "s3/core/physics/board_animation.hpp"
#include "s3/core/physics/board_runtime.hpp"
#include "s3/core/physics/drive_frames.hpp"
#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::skeleton_air {

using Transform = sc::physics::skeleton_animation_record::AnimationPartTransform;

///The original board's persistent blend history, shared across Ground/Air
///state changes. This must live alongside the physical board, not per entry.
struct SkeletonAir {
    sc::physics::board_animation::BoardAnimation board_animation{};
    // Private in Rust.
    sc::physics::board_animation::BoardAnimationSettings settings{};

    static rs::Result<SkeletonAir, std::string> load(const sd::Collections& data);

    ///Original82D38000 GroundUpdate ->82D38008 UpdatePhysicsError. Call after
    ///Skeleton Ground GeneralUpdate and before the physical solve, every tick.
    void capture_physics_error(const sc::physics::board_runtime::BoardRuntime& board,
                               const Transform& animated_target);

    ///82C0606C; preserves the source's independently retained blending flag.
    void reset_board();

    ///82C041C0: publish the effective matrix to the separate deck constraint
    ///anchor. It does not teleport the dynamic board assembly.
    Transform apply_board(sc::physics::board_runtime::BoardRuntime& board, const Transform& target, bool fast);
};

/// Private in Rust.
Transform matrix(sc::physics::drive_frames::RetailAffineTransform t);
/// Private in Rust.
sc::physics::drive_frames::RetailAffineTransform affine(const Transform& m);

}  // namespace s3::game::physics::skeleton_air
