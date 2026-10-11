// Port of skate-game/src/physics/offboard/ground_geometry.rs
//! Ground Sync submits edge geometry; the next PreUpdate consumes its result.
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "s3/core/math.hpp"
#include "s3/core/physics/board_world.hpp"
#include "s3/core/player/offboard/ground_query.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/offboard/ground_sync.hpp"

namespace s3::game::physics::offboard::ground_geometry {

namespace query = ::s3::player::offboard::ground_query;
using query::ConsumeInput;
using query::Frame;
using query::GroundAdjustment;
using query::GroundQueryPacket;
using query::LineHit;
using query::QueryContext;
using sc::math::Vector3;
using sc::physics::board_world::BoardWorld;

struct State {
    // Private in Rust.
    std::optional<std::pair<GroundQueryPacket, std::array<std::optional<LineHit>, 7>>> pending;
    float collision_offset = 0.0f;

    static rs::Result<State, std::string> load(const sd::Collections& data);
    void reset();
    GroundAdjustment consume(ConsumeInput input);
    Status submit(const BoardWorld& world, std::array<std::array<float, 4>, 4> frame, std::array<float, 4> velocity,
                  QueryContext context, std::uint32_t flags_2488);
};

Frame query_frame(std::array<std::array<float, 4>, 4> frame);
Vector3 xyz(std::array<float, 4> v);
std::array<float, 4> lanes(Vector3 v);
std::array<std::array<float, 4>, 4> native_frame(Frame frame);

}  // namespace s3::game::physics::offboard::ground_geometry
