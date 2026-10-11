// Port of skate-game/src/physics/offboard/board_manager/runtime.rs
//! Live bindings for TU3 SkateboardController, 82DB6150 and 82D76D20.
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>

#include "s3/core/physics/board.hpp"
#include "s3/core/physics/board_step.hpp"
#include "s3/core/physics/contact.hpp"
#include "s3/core/player/offboard/board_possession.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/fwd.hpp"
#include "s3/game/physics/offboard/board_possession.hpp"

namespace s3::game::physics::offboard::board_manager::runtime {

using board_possession::Effects;
using board_possession::VolumeFlags;
using sc::physics::board_step::CollisionBody;
using sc::physics::contact::RetailContactMaterial;
using sc::player::offboard::board_possession::Fill;
using sc::player::offboard::board_possession::lifecycle::Alignment;
using sc::player::offboard::board_possession::lifecycle::Observation;

struct LiveState {
    VolumeFlags volumes{};
    // Private in Rust.
    Alignment alignment{};
    bool alignment_active = false;
    std::array<RetailContactMaterial, 3> standard_materials{};
    RetailContactMaterial released_material{};
    float standard_drag = 0.0f;
    // pub
    std::optional<Fill> output;

    static rs::Result<LiveState, std::string> load(const sd::Collections& data, const GamePhysics& physics);
    Effects effects(GamePhysics& physics, std::uint8_t& animated, float dt);
    /// The compound children and shared truck shape are the actual fields
    /// colliders::world_volumes consumes. Wheel/aggregate gates are applied in
    /// solve.rs before either world queries or board/skater pairing.
    void publish_volumes(GamePhysics& physics) const;
    bool volume_enabled(CollisionBody body) const;
};

Observation observe(const GamePhysics& physics, const SkaterRuntime& skater);
///82C08818 votes only four wheel surfaces; physical contact counts four,
/// line-only support counts one. Ties retain the lowest nonzero surface.
/// private
std::uint32_t surface(const GamePhysics& physics);
/// Insert immediately after frame.rs's selected physical-state Update match,
/// before state_timer_1344 and solve. TU3 82DB6150..61B0.
void update(GamePhysics& physics, SkaterRuntime& skater);
///82DB8998 brackets board/skeleton teleport with this operation. Call before
///board.reset_physical, then again after Skeleton update_teleport.
void reset_for_teleport(GamePhysics& physics, SkaterRuntime& skater);
///82DB8E10..8E24, AFTER teleport's SetPhysicsState(Ground). Standard-board
///restoration must follow Stop's released-material writes, not precede them.
void finish_teleport(GamePhysics& physics, SkaterRuntime& skater);
/// Insert after finish_skater (completed contacts), before next input. The
/// alignment test is82C0837C..8428 and publishes the actual release bit872.
Fill publish(GamePhysics& physics, SkaterRuntime& skater);

}  // namespace s3::game::physics::offboard::board_manager::runtime
