// Port of skate-game/src/physics/skeleton_output.rs.
//! Production adapter for the original physical-pose job82DB6698/82DB95E8.
#pragma once
#include <array>
#include <string>

#include "s3/core/animation/slice.hpp"
#include "s3/core/physics/board_runtime.hpp"
#include "s3/core/physics/drive_frames.hpp"
#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/core/physics/skeleton_body.hpp"
#include "s3/core/physics/skeleton_output.hpp"
#include "s3/core/physics/skeleton_output/board.hpp"
#include "s3/core/physics/skeleton_output/correction.hpp"
#include "s3/core/physics/skeleton_output/wobble.hpp"
#include "s3/data/animation_banks.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::animated_skeleton {
struct AnimatedSkeleton;
}

namespace s3::game::physics::skeleton_output {

using Transform = sc::physics::skeleton_animation_record::AnimationPartTransform;

struct SkeletonOutput {
    sc::physics::skeleton_output::SkeletonOutput pose{};
    sc::physics::skeleton_output::wobble::Wobble wobble{};
    // Private in Rust.
    sc::physics::skeleton_output::wobble::Settings wobble_settings{};
    // Public in Rust.
    sc::physics::skeleton_output::wobble::Output deck_wobble{};
    sc::physics::skeleton_output::correction::CorrectionState correction{};
    // Private in Rust.
    float compression_rest_height = 0.0f;

    static rs::Result<SkeletonOutput, std::string> load(const sd::Collections& data, const sd::AnimationFrames& animation,
                                                        const animated_skeleton::AnimatedSkeleton& skeleton);
    ///82BF2F18 is called on the actual landing/takeoff transition; reverse is
    ///the native stance decision passed by that transition's caller.
    void trigger_wobble(bool landing, bool reverse);
    ///82C08968 observes the solved COM frames, not geometric part frames.
    std::array<float, 2> average_compressions(const sc::physics::board_runtime::BoardRuntime& board) const;
    ///Call in82BD83E0's post-wipeout-check phase, before the pose job.
    Transform advance_wobble(sc::physics::skeleton_body::SkeletonBody& physical, const Transform& deck);
    Status publish(const animated_skeleton::AnimatedSkeleton& skeleton,
                   const sc::physics::skeleton_body::SkeletonBody& physical,
                   const sc::physics::board_runtime::BoardRuntime& board_runtime,
                   std::array<sc::physics::drive_frames::RetailAffineTransform, 2> base_trucks,
                   std::array<float, 2> current_truck_targets, std::array<float, 2> average_compression,
                   sc::animation::Slice<Transform> globals, sc::animation::Slice<Transform> locals) const;
};

/// Private in Rust.
Transform to_matrix(sc::physics::drive_frames::RetailAffineTransform value);

}  // namespace s3::game::physics::skeleton_output
