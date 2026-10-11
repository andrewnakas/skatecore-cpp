// Port of skate-game/src/physics/ground_runtime/update.rs.
//! Ordinary Ground82D37C88 executes persistent controller updates and actual
//!body forces before prediction and IK publication. Ground's handplant query
//!82D38430 is dispatched by ground_phase through the retained Handplant owner.
//! (`impl GroundState::update` is declared in GroundState.)
#pragma once
#include <array>
#include <functional>

#include "s3/core/math.hpp"
#include "s3/core/physics/board_toolkit.hpp"
#include "s3/core/physics/drive_frames.hpp"
#include "s3/core/player/input_phase.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/ground_runtime/input.hpp"

namespace s3::game::physics::riding_outputs {
struct RidingOutputs;  // physics/riding_outputs.hpp (group D)
}
namespace s3::game::physics::animated_skeleton {
struct AnimatedSkeleton;  // physics/animated_skeleton.hpp (group D)
}
namespace s3::game::physics::animation_input {
struct AnimationInput;  // physics/animation_input.hpp (group D)
}
namespace s3::game::physics::foot_ik {
struct FootIk;  // physics/foot_ik.hpp (group D)
}
namespace s3::game::physics::biped_ground::grab_runtime {
struct Owner;  // physics/biped_ground/grab_runtime.hpp (group A)
}

namespace s3::game::physics::ground_runtime::update {

struct GroundUpdateFrame {
    const sc::player::input_phase::ProcessedPhysicsInput& processed;
    const animation_input::AnimationInput& animation;
    const riding_outputs::RidingOutputs& riding;
    const animated_skeleton::AnimatedSkeleton& skeleton;
    const sc::physics::board_toolkit::BoardToolkit& toolkit;
    std::array<sc::physics::drive_frames::RetailAffineTransform, 2> base_trucks;
    input::GroundInputObservations extra;
};
struct GroundUpdateTargets {
    foot_ik::FootIk& foot_ik;
    bool& skeleton_elapsed_16505;
    /// Borrow the canonical SkeletonOutput correction latch (Skeleton16388).
    bool& board_correction_pending;
    ///Applies to SkeletonDrives hook432 body and Skeleton16112 together.
    /// (Rust `&mut dyn FnMut(Vector3) -> Result<(), String>`.)
    std::function<Status(sc::math::Vector3)> move_future_deck;
    biped_ground::grab_runtime::Owner& offboard_grab;
};

}  // namespace s3::game::physics::ground_runtime::update
