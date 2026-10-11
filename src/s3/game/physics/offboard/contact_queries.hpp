// Port of skate-game/src/physics/offboard/contact_queries.rs
//! Scene boundary for the recovered BipedToolkit probe layout. The pending
//! packet owns observations; the world's geometry and acceleration tree stay shared.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "s3/core/math.hpp"
#include "s3/core/physics/board_world.hpp"
#include "s3/core/physics/drive_frames.hpp"
#include "s3/core/physics/triangle_query.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::offboard::contact_queries {

using sc::math::Vector3;
using sc::physics::board_world::BoardWorld;
using sc::physics::drive_frames::RetailAffineTransform;
using sc::physics::triangle_query::TriangleLineHit;

/// #[derive(Clone, Copy)]
struct Probe {
    std::array<float, 4> start{};
    std::array<float, 4> end{};
    float radius = 0.0f;
};

/// #[derive(Clone, Copy, Debug)]
struct Hit {
    TriangleLineHit geometry{};
    std::uint16_t packed_surface = 0;
    std::size_t mesh_index = 0;
    RetailAffineTransform support_frame{};
};

/// private
Vector3 xyz(std::array<float, 4> v);
rs::Result<std::optional<Hit>, std::string> query(const BoardWorld& world, Probe probe, std::uint32_t matching_id);

}  // namespace s3::game::physics::offboard::contact_queries
