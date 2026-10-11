// Port of skate-game/src/physics/teleport_state.rs.
//! Host checkpoint response to original actor reset callback82592518.
//! Shared scheduling preserves request -> next input reply ->702 output -> reset.
#pragma once
#include <array>
#include <optional>
#include <utility>

#include "s3/core/animation/output/actor_packet.hpp"
#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/core/player/input_phase.hpp"
#include "s3/core/player/teleport_state.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::teleport_state {

using sc::animation::output::actor_packet::ExternalReset;
using sc::physics::skeleton_animation_record::AnimationPartTransform;
using sc::player::teleport_state::Output;
using sc::player::teleport_state::Target;
using sc::player::teleport_state::TeleportState;
using sc::player::teleport_state::Update;

/// `#[derive(Clone, Copy, Debug)]`
struct Checkpoint {
    AnimationPartTransform transform{};
    /// Original actor82592518 inverts checkpoint byte68. Its ground validator
    ///82BFBC18 sets that byte for surface category8. Authored rideable spawn
    ///surfaces supply true; an off-board checkpoint must explicitly supply false.
    bool on_board = false;
};

struct Runtime {
    TeleportState state{};
    Checkpoint checkpoint{};
    std::optional<Target> pending_reply;
    std::optional<bool> manual_on_board;
    std::optional<std::pair<std::array<float, 3>, std::array<float, 3>>> vehicle_ejection;

    /// Explicit manual return uses the actor-reset publication without replacing
    /// the automatic recovery checkpoint owned by this runtime.
    void request_manual(AnimationPartTransform transform, bool on_board);
    void request_vehicle_ejection(AnimationPartTransform transform, std::array<float, 3> velocity,
                                  std::array<float, 3> angular);
    std::optional<std::pair<std::array<float, 3>, std::array<float, 3>>> take_vehicle_ejection();
    std::optional<bool> take_manual_on_board();
    /// `pub(super)`.
    void set_checkpoint(Checkpoint checkpoint);
    static Runtime new_(Checkpoint checkpoint);
    void enter();
    /// Update82D431F0 calls the actor only without Processed2468 bit1.
    ///825926F8 stores the reply and marks it for the next input publication.
    bool update(const sc::player::input_phase::ProcessedPhysicsInput& input);
    /// Host response for an independently verified actor-reset request.
    void request_checkpoint();
    void reply(Checkpoint checkpoint);
    /// Consume at the next actor-reset publication boundary. Publish this as
    ///ExternalReset plus Actor1904 bit29, then use the ordinary animation-packet
    ///mapper to produce1536/1600 and2468 bit1. Never reset physical bodies here.
    std::optional<ExternalReset> take_reply();
    /// Call after ordinary physical publication so702 owns State8/61 and
    ///the returned board0 transform/board272 publication for this tick.
    std::optional<Output> publish_output(sc::player::input_phase::PhysicalPlayerInput& physical) const;
};

}  // namespace s3::game::physics::teleport_state
