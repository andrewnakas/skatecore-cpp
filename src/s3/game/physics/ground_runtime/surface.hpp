// Port of skate-game/src/physics/ground_runtime/surface.rs.
//! Native wheel surface vote82C08818 on live wheel lines and contact reports.
#pragma once
#include <array>
#include <cstdint>
#include <string>

#include "s3/core/physics/board_runtime.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::riding_outputs {
struct RidingOutputs;  // physics/riding_outputs.hpp (group D)
}

namespace s3::game::physics::ground_runtime::surface {

using riding_outputs::RidingOutputs;

/// Wheel surface IDs come from line queries. The report loop82C08198 updates
/// only truck/deck material slots, so wheel contacts contribute vote weights
/// without replacing the query-owned material ID.
std::uint32_t active_surface(const RidingOutputs& riding, const sc::physics::board_runtime::BoardRuntime& board);

/// Select the authored audio material from the same four retained wheel queries. Skate 3 packs
/// this independently from the physics surface: audio occupies bits 0..6, while friction uses
/// bits 7..11. Contacting wheels receive the retail four-vote weight and ties prefer the lower ID.
std::uint32_t active_audio_surface(const RidingOutputs& riding);

/// Private in Rust (exposed for the ported tests).
std::uint32_t choose_audio_surface(std::array<std::uint32_t, 4> surfaces, std::array<bool, 4> contacts);

/// SurfacePhysics constructor82D85D08 binds these exact five collection keys
/// at24/40/56/72/88. Lookup8 hashes independently match the native immediates.
/// The selector's mode normalization lives in the processed-input producer.
rs::Result<const char*, std::string> surface_key(std::uint32_t mode);

}  // namespace s3::game::physics::ground_runtime::surface
