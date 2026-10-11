// Port of skate-game/src/physics/player_input/services.rs.
//! Host ownership for the original 82DB4048 input phase. Every numerical
//! preparation is performed here; the two callbacks address the shared
//! skeleton/whole-player owners that cannot be borrowed twice by the host.
#pragma once
#include <cstdint>
#include <optional>
#include <string>

#include "s3/core/physics/board_runtime.hpp"
#include "s3/core/physics/board_toolkit.hpp"
#include "s3/core/physics/board_world.hpp"
#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/core/player/input_phase.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/player_input/grind.hpp"
#include "s3/game/physics/player_input/pre_input.hpp"

namespace s3::game::physics::ground_runtime {
struct GroundRuntime;  // physics/ground_runtime.hpp
}

namespace s3::game::physics::player_input::services {

using ground_runtime::GroundRuntime;
using sc::physics::board_runtime::BoardRuntime;
using sc::physics::board_toolkit::BoardToolkit;
using sc::physics::board_world::BoardWorld;
using sc::physics::skeleton_animation_record::AnimationPartTransform;
using sc::player::input_phase::AnimationInputPacket;
using sc::player::input_phase::PhysicalPlayerInput;
using sc::player::input_phase::PlayerInputState;
using sc::player::input_phase::ProcessedPhysicsInput;
using sc::player::input_phase::RawVector;

/// `#[derive(Clone, Copy)]`
struct InputHostFrame {
    std::uint32_t actor_query_56 = 0;
    std::uint32_t actor_query_44 = 0;
    bool input_available = false;
    /// Actual gameplay action 71 from ControllerInput::player_actions().
    float transition_action = 0.0f;
    /// Motion output's published +0 frame, read by the native reset request.
    AnimationPartTransform published_board_transform{};
    /// Native air-state manager +40, only read by the category200 grind gate.
    std::int32_t air_counter_40 = 0;
};

/// Rust trait `PlayerInputCallbacks` -> abstract interface.
struct PlayerInputCallbacks {
    virtual ~PlayerInputCallbacks() = default;
    virtual rs::Result<grind::Pending, std::string> prepare_grind(BoardRuntime& board, grind::GrindInputState& grind,
                                                                  const ProcessedPhysicsInput& processed,
                                                                  const BoardWorld& world,
                                                                  const grind::StaticProvider& provider,
                                                                  std::int32_t air_counter) = 0;
    virtual Status process_skeleton(BoardRuntime& board, const BoardToolkit& toolkit,
                                    const AnimationInputPacket& packet, PhysicalPlayerInput& physical,
                                    ProcessedPhysicsInput& processed) = 0;
    /// Complete 82DB8998 reset including board, skeleton, Reckoning, current
    /// physical outputs and Ground entry. Returning an error retains the request.
    virtual Status teleport(BoardRuntime& board, GroundRuntime& ground, AnimationPartTransform target,
                            PlayerInputState& player, PhysicalPlayerInput& physical,
                            ProcessedPhysicsInput& processed) = 0;
};

/// `pub(super) struct Services<'a, C>`; `impl InputPhaseServices for Services` (Error = String).
/// The generic callbacks parameter becomes the abstract interface.
struct Services final : sc::player::input_phase::InputPhaseServices<std::string> {
    BoardRuntime& board;
    GroundRuntime& ground;
    AnimationPartTransform ground_frame;
    InputHostFrame host;
    PlayerInputCallbacks& callbacks;
    pre_input::PreInputManager& pre_input;
    grind::GrindInputState& grind;
    const grind::StaticProvider& world_edges;
    const BoardWorld& world;
    std::optional<grind::Pending>& pending_grind;
    std::optional<BoardToolkit>& toolkit;
    std::optional<AnimationPartTransform>& pending_teleport;

    Services(BoardRuntime& board, GroundRuntime& ground, AnimationPartTransform ground_frame, InputHostFrame host,
             PlayerInputCallbacks& callbacks, pre_input::PreInputManager& pre_input, grind::GrindInputState& grind,
             const grind::StaticProvider& world_edges, const BoardWorld& world,
             std::optional<grind::Pending>& pending_grind, std::optional<BoardToolkit>& toolkit,
             std::optional<AnimationPartTransform>& pending_teleport)
        : board(board),
          ground(ground),
          ground_frame(ground_frame),
          host(host),
          callbacks(callbacks),
          pre_input(pre_input),
          grind(grind),
          world_edges(world_edges),
          world(world),
          pending_grind(pending_grind),
          toolkit(toolkit),
          pending_teleport(pending_teleport) {}

    R<rs::Unit> update_pre_input_manager_82d81610(PlayerInputState& player, PhysicalPlayerInput& physical) override;
    R<rs::Unit> reset_processed_input_82bf9ef0(ProcessedPhysicsInput& output) override;
    R<std::uint32_t> actor_query_slot_56() override;
    R<std::uint32_t> actor_query_slot_44() override;
    R<rs::Unit> reset_player_probe_82d7a330(PlayerInputState& player, PhysicalPlayerInput& physical) override;
    R<rs::Unit> check_teleport_82db88c8(PlayerInputState& player, PhysicalPlayerInput& physical,
                                        ProcessedPhysicsInput& output) override;
    R<bool> actor_input_available_slot_4() override;
    R<float> transition_action_825903c8() override;
    R<RawVector> calculate_ground_position_82c02840(const PhysicalPlayerInput& physical) override;
    R<rs::Unit> prepare_board_toolkit_82c013f0(PlayerInputState& player, PhysicalPlayerInput& physical,
                                               ProcessedPhysicsInput& output) override;
    R<rs::Unit> process_skeleton_82bd8918(const AnimationInputPacket& packet, PhysicalPlayerInput& physical,
                                          ProcessedPhysicsInput& output) override;
    R<rs::Unit> update_grind_manager_82d8a828(PlayerInputState& player, PhysicalPlayerInput& physical,
                                              ProcessedPhysicsInput& output) override;
};

}  // namespace s3::game::physics::player_input::services
