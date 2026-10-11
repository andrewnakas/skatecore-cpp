// Port of skate-game/src/physics/player_input/mod.rs.
//! Concrete original Skate3 player input phase for the authored riding world.
#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <variant>

#include "s3/core/air/trajectory.hpp"
#include "s3/core/math.hpp"
#include "s3/core/physics/board_dynamic_normal.hpp"
#include "s3/core/physics/board_input_output.hpp"
#include "s3/core/physics/board_runtime.hpp"
#include "s3/core/physics/board_toolkit.hpp"
#include "s3/core/physics/board_world.hpp"
#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/core/physics/skeleton_body.hpp"
#include "s3/core/player/input_phase.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"
#include "s3/game/grind_world.hpp"
#include "s3/game/physics/grind/observation.hpp"
#include "s3/game/physics/player_input/dynamic_normal.hpp"
#include "s3/game/physics/player_input/grind.hpp"
#include "s3/game/physics/player_input/ground_position.hpp"
#include "s3/game/physics/player_input/initial.hpp"
#include "s3/game/physics/player_input/output_reset.hpp"
#include "s3/game/physics/player_input/pre_input.hpp"
#include "s3/game/physics/player_input/reset.hpp"
#include "s3/game/physics/player_input/services.hpp"

namespace s3::game::physics::riding_outputs {
struct RidingOutputs;  // physics/riding_outputs.hpp (group D)
}

namespace s3::game::physics::player_input {

using output_reset::reset_outputs;
using services::InputHostFrame;
using services::PlayerInputCallbacks;
using sc::physics::skeleton_animation_record::AnimationPartTransform;

/// Rust `enum InputStage { ThroughTeleport, AfterTeleport(InputContinuation) }`.
struct InputStage {
    struct ThroughTeleport {};
    struct AfterTeleport {
        sc::player::input_phase::InputContinuation _0;
    };
    std::variant<ThroughTeleport, AfterTeleport> value;

    InputStage(ThroughTeleport v) : value(v) {}
    InputStage(AfterTeleport v) : value(std::move(v)) {}
};

struct PlayerInputRuntime {
    sc::player::input_phase::PlayerInputState player{};
    sc::player::input_phase::PhysicalPlayerInput physical{};
    sc::player::input_phase::ProcessedPhysicsInput processed{};
    std::optional<sc::physics::board_toolkit::BoardToolkit> toolkit;
    sc::physics::board_dynamic_normal::BoardDynamicNormal dynamic_normal{};
    sc::physics::board_dynamic_normal::DynamicNormalSettings normal_settings{};
    pre_input::PreInputManager pre_input{};
    grind::GrindInputState grind{};
    /// Rust field `pending_teleport` (renamed: clashes with method `pending_teleport()`).
    std::optional<AnimationPartTransform> pending_teleport_;
    std::optional<grind::Pending> pending_grind;
    std::optional<::s3::game::physics::grind::observation::ManagerObservation> grind_observation;

    std::optional<AnimationPartTransform> pending_teleport() const { return pending_teleport_; }
    Status request_teleport(AnimationPartTransform target);
    static rs::Result<PlayerInputRuntime, std::string> load(const sd::Collections& data);
    sc::player::input_phase::ProcessedPhysicsSnapshot processed_snapshot(std::uint64_t tick) const;
    ///Run after the current board/contact solve, before ProcessOutput publishes.
    void update_dynamic_normal(const riding_outputs::RidingOutputs& riding, sc::math::Vector3 gravity);
    ///Reset and all other component publications are coordinator-owned phases.
    Status publish_board(const riding_outputs::RidingOutputs& riding);
    /// Rust generic `C: PlayerInputCallbacks` -> abstract interface.
    rs::Result<std::optional<sc::player::input_phase::InputContinuation>, std::string> process_stage(
        sc::physics::board_runtime::BoardRuntime& board, ground_runtime::GroundRuntime& ground,
        AnimationPartTransform ground_frame, const sc::player::input_phase::AnimationInputPacket& packet,
        InputHostFrame host, PlayerInputCallbacks& callbacks, InputStage stage,
        const sc::physics::board_world::BoardWorld& world, const ::s3::game::grind_world::StaticProvider& world_edges);

    // ---- grind_output.rs ----
    /// After output reset and final physical-record correction, before the
    /// conditioner/graphs. Board publication separately supplies Ground80 and
    /// the host-stored Motion273 flag. The selector remains the sole Air443 owner.
    Status publish_grind_graph_outputs(const sc::physics::skeleton_body::SkeletonPhysicalRecord& record,
                                       std::array<float, 4> reckoning_up,
                                       const sc::air::trajectory::TrajectorySelector& selector);
};

/// Private helper.
sc::math::Vector3 xyz(std::array<float, 4> v);

}  // namespace s3::game::physics::player_input
