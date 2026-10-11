// Port of skate-game/src/physics/wipeout/observations.rs
//! Borrow the actual completed collision and physical owners; no default contacts.
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>

#include "s3/core/air/reckoning.hpp"
#include "s3/core/math.hpp"
#include "s3/core/physics/board_ground.hpp"
#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/core/physics/skeleton_body.hpp"
#include "s3/core/physics/skeleton_body/collision_feedback.hpp"
#include "s3/core/player/input_phase.hpp"
#include "s3/core/player/wipeout.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::wipeout::observations {

using ::s3::air::reckoning::AirState;
using ::s3::math::Vector3;
using ::s3::physics::board_ground::BoardGroundState;
using ::s3::physics::skeleton_animation_record::AnimationPartTransform;
using ::s3::physics::skeleton_body::collision_feedback::SkeletonCollisionFeedback;
using ::s3::player::input_phase::ProcessedPhysicsInput;
using ::s3::player::wipeout::Frame;
using ::s3::player::wipeout::RequestInput;

struct Observations {
    const ProcessedPhysicsInput& processed;
    const BoardGroundState& board;
    const SkeletonCollisionFeedback& collision;
    AnimationPartTransform deck;
    ///Actual Processed0/16/32 publication, supplied by its retained producer.
    AnimationPartTransform input_board;
    AnimationPartTransform world_to_animation;
    std::array<float, 4> pose_error;
    ///Skeleton16384 is unavailable before the first actual postphysics feedback.
    std::optional<float> maximum_pose_error;
    std::uint32_t jump_fix_frames;
    const AirState& air;
    ///Reckoning816's up column Y, original832.Y.
    float system_up_y;
    bool grind_locked_to_middle;
    std::optional<std::array<float, 4>> grind_normal;

    rs::Result<Frame, std::string> frame() const;
};

RequestInput request_input(const ProcessedPhysicsInput& p);
std::array<float, 4> vector(Vector3 v);

}  // namespace s3::game::physics::wipeout::observations
