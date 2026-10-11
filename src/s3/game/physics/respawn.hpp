// Port of skate-game/src/physics/respawn.rs.
//! TU3 actor checkpoint manager, bound to the single-player static scene.
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>

#include "s3/core/physics/board_world.hpp"
#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/core/player/respawn.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/fwd.hpp"

namespace s3::game::physics::respawn {

using Matrix = sc::physics::skeleton_animation_record::AnimationPartTransform;
using sc::physics::board_world::BoardWorld;
using sc::player::respawn::Ground;
using sc::player::respawn::History;

/// Private in Rust.
struct Settings {
    float height = 0.0f;
    float radius = 0.0f;
    float drop = 0.0f;
    float normal_y = 0.0f;
    std::uint32_t minimum_frames = 0;
};

/// `pub(super)`.
struct Runtime {
    History history;
    Settings settings{};
    std::int32_t measurements = 0;

    static rs::Result<Runtime, std::string> load(const sd::Collections& data, Matrix transform,
                                                 std::uint32_t stance);
    void reset_measurements();
};

///Actor82592518 saves stance then queues the ordinary deferred reply825926F8.
Status request(const GamePhysics& physics, SkaterRuntime& skater);
/// `pub(super)`.
Status observe(const GamePhysics& physics, SkaterRuntime& skater);

/// Private helpers.
void flip(Matrix& matrix);
///82591E30 retains the source axes at low speed or near vertical travel.
Matrix heading(Matrix matrix, std::array<float, 4> velocity);

/// Private: `impl Validation for Scene` (duck-typed, Error = String).
struct Scene {
    const BoardWorld& world;
    const Settings& settings;

    using Error = std::string;
    rs::Result<std::optional<Ground>, std::string> ground(const Matrix& transform);
    rs::Result<bool, std::string> location(const Matrix& transform);
    rs::Result<bool, std::string> occupants(const Matrix& transform);
    rs::Result<bool, std::string> edges(const Matrix& transform);
};

}  // namespace s3::game::physics::respawn
