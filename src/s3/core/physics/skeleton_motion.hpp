// Port of skate-core/src/physics/skeleton_motion.rs.
// Trajectory and board-orientation publications inside ProcessData82BD8918.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/physics/skeleton_animation_record.hpp"

namespace s3::physics::skeleton_motion {

using skeleton_animation_record::AnimationPartTransform;

class SkeletonMotion {
public:
    // Default: 82BD7260 seeds the three frames from82139A10/20/30 and zero pos.
    ///Skeleton12048,12112,12176. Later skeleton modes may set the next frame.
    AnimationPartTransform trajectory = skeleton_animation_record::IDENTITY;
    AnimationPartTransform inverse_trajectory = skeleton_animation_record::IDENTITY;
    AnimationPartTransform next_trajectory = skeleton_animation_record::IDENTITY;
    ///Skeleton16320: current trajectory translation/dt, transformed to world.
    std::array<float, 4> velocity_world{};

    static SkeletonMotion default_() { return SkeletonMotion{}; }

    ///82BE3538 resets only the board-orientation history, not trajectory.
    void reset_board_orientation_history();

    ///82BD89D0..8BA0. Bone0 is the source trajectory, not the animated foot.
    ///Preserve the two reciprocal refinements after the shared estimate model.
    void process_trajectory(const AnimationPartTransform& trajectory_bone,
                            const AnimationPartTransform& animation_to_world, float dt);

    ///82BD8CB0..8D2C precedes the landing/board-offset adjustment. Only sets
    ///the side-on flag; this stage does not clear an already published flag.
    static void publish_unadjusted_board(const AnimationPartTransform& board, std::uint32_t& flags_2472);

    ///82BD8E14..8E50 uses board At.y, not height or translation. Returns the
    ///difference published to Processed2768 and updates its complementary bits.
    float publish_adjusted_board(const AnimationPartTransform& board, std::uint32_t& flags_2468);

private:
    ///Skeleton16500, initialized zero by82BD7A6C.
    float previous_board_at_y = 0.0f;
};

}  // namespace s3::physics::skeleton_motion
