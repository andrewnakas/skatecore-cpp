// Port of skate-game/src/physics/ground_phase.rs.
//! Production Ground entry82D37538 and update82D37C88/82D38800.
//! State selection belongs to the player coordinator. This phase consumes its
//! current processed packet and changes the same board that the solver advances.
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>

#include "s3/core/air/reckoning.hpp"
#include "s3/core/math.hpp"
#include "s3/core/physics/board_runtime.hpp"
#include "s3/core/physics/board_toolkit.hpp"
#include "s3/core/physics/skeleton_body.hpp"
#include "s3/core/player/input_phase.hpp"
#include "s3/core/player/wipeout.hpp"
#include "s3/core/riding/grounded/state/board.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/fwd.hpp"
#include "s3/game/physics/ground_runtime/launch.hpp"
#include "s3/game/physics/skeleton_controller.hpp"

namespace s3::game::physics::ground_runtime {
struct GroundRuntime;  // physics/ground_runtime.hpp
namespace state {
struct GroundState;  // physics/ground_runtime/state.hpp
}
}  // namespace s3::game::physics::ground_runtime
namespace s3::game::physics::animation_input {
struct AnimationInput;  // physics/animation_input.hpp (group D)
}
namespace s3::game::physics::foot_ik {
struct FootIk;  // physics/foot_ik.hpp (group D)
}
namespace s3::game::physics::skeleton_output {
struct SkeletonOutput;  // physics/skeleton_output.hpp (group D)
}

namespace s3::game::physics::ground_phase {

using sc::math::Vector3;

/// Edge observations are owned geometry results, not inferred from the deck.
/// The current authored world explicitly has no grind edges.
/// `#[derive(Clone, Copy)]`
struct GroundEdge {
    std::uint32_t flags = 0;
    std::array<float, 4> point{};
    Vector3 start{};
    Vector3 end{};
};

struct GroundLifecycle {
    skeleton_controller::SkeletonControllerState skeleton_controller =
        skeleton_controller::SkeletonControllerState::new_();
    /// Retained Skeleton lifecycle flag, also shared with teleport.
    /// Skeleton16388 instead belongs solely to SkeletonOutput::correction.
    bool skeleton_elapsed_16505 = false;
    std::uint8_t board_animated_290 = 0;
    /// Processed2724: Reset82BFA35C clears this. The original image contains
    /// no other direct scalar writer at that offset.
    float manual_drag_2724 = 0.0f;
    std::optional<GroundEdge> edge;
    /// A selected wall jump is retained for its actual selector continuation.
    /// It cannot be discarded as if Ground's ordinary tail had completed.
    std::optional<ground_runtime::launch::GroundLaunchInfo> pending_wall_jump;

    static GroundLifecycle new_();
};

/// Typed portion of Skateboard Reset82C05F50, alongside the coordinator's
/// actual body/pose/force reset. Native timer fields8364/8368 are not written
/// by either this reset or ResetBoardBody82C0D680, so preserve activation_time.
void reset_board_state(ground_runtime::state::GroundState& ground, ground_runtime::GroundRuntime& runtime,
                       GroundLifecycle& life, bool& board_wiping_out);

/// Called only on the coordinator's real transition into Ground. Entry needs
/// the toolkit already produced by this tick's PlayerInput phase.
Status enter(GamePhysics& physics, SkaterRuntime& skater);

/// The same entry for the reset callback while PlayerInput owns its mutable
/// processed packet. Splitting host borrows does not alter the source order.
Status enter_components(sc::physics::board_runtime::BoardRuntime& board,
                        const sc::player::input_phase::ProcessedPhysicsInput& processed,
                        const animation_input::AnimationInput& animation,
                        const sc::physics::board_toolkit::BoardToolkit& toolkit,
                        ground_runtime::state::GroundState& ground, GroundLifecycle& life,
                        sc::air::reckoning::AirState& air_reckoning, sc::player::wipeout::Requests& wipeout,
                        sc::physics::skeleton_body::SkeletonCollisionMode& collision_mode, foot_ik::FootIk& ik,
                        skeleton_output::SkeletonOutput& output, bool& board_wiping_out);

/// Run after the Ground Reckoning preupdate and before the shared body solve.
/// The caller passes ground.steering.targets to that solve and must not clear
/// the board's force queue between this call and the solve.
rs::Result<sc::riding::grounded::state::board::GroundBoardOutcome, std::string> advance(GamePhysics& physics,
                                                                                         SkaterRuntime& skater);

}  // namespace s3::game::physics::ground_phase
