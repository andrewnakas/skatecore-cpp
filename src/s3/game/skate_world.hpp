// Port of skate-game/src/skate_world.rs (simulation half, as kept by simref).
//! SIMREF: simulation half of skate-game/src/skate_world.rs (lines 1-376). Rendering/spawn code removed.
#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <utility>

#include "s3/core/physics/board_world.hpp"
#include "s3/core/physics/contact.hpp"
#include "s3/data/skate_map.hpp"
#include "s3/game/common.hpp"

namespace s3::game::skate_world {

Status validate_runtime(const sd::SkateMap& map);

/// TU3 ClusteredMesh::GetUnitVolumes (82AC8A68): fdivs then fsubs,
/// using the pi-squared word at 822F88D0. This is not acos/angle decoding.
/// Bit 7 denotes an unmatched compiler edge and is not a triangle flag.
/// (Private in Rust.)
rs::Result<std::pair<std::uint32_t, std::array<float, 3>>, std::string> decode_native_edges(
    std::array<std::uint8_t, 3> edges);

rs::Result<sc::physics::board_world::BoardWorld, std::string> collision_world(
    const sd::SkateMap& map, sc::physics::contact::RetailContactMaterial material);

}  // namespace s3::game::skate_world
