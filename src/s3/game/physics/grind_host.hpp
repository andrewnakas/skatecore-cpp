// Port of skate-game/src/physics/grind_host.rs.
//! Bind the manager to the current board materials and authored world queries.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "s3/core/air/trajectory/grind_surface.hpp"
#include "s3/core/physics/board_runtime.hpp"
#include "s3/core/physics/board_world.hpp"
#include "s3/core/physics/grind_contact/balance.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/grind_materials.hpp"
#include "s3/game/physics/player_input/grind.hpp"  // Host, MaterialMode (group C)

namespace s3::game::physics::grind_host {

using grind_materials::GrindMaterials;
using player_input::grind::MaterialMode;
using ::s3::air::trajectory::grind_surface::Probe;
using ::s3::air::trajectory::grind_surface::ProbeHit;
using ::s3::physics::board_runtime::BoardRuntime;
using ::s3::physics::board_world::BoardWorld;
namespace balance = ::s3::physics::grind_contact::balance;

/// `impl Host for LiveHost<'_>`.
struct LiveHost final : player_input::grind::Host {
    BoardRuntime& board;
    settings::PhysicsSettings& settings;
    const GrindMaterials& materials;

    LiveHost(BoardRuntime& board_, settings::PhysicsSettings& settings_, const GrindMaterials& materials_)
        : board(board_), settings(settings_), materials(materials_) {}

    Status apply_material_mode(MaterialMode mode) override;
    rs::Result<std::optional<ProbeHit>, std::string> surface_probe(const BoardWorld& world,
                                                                   std::array<std::uint32_t, 2> actor,
                                                                   std::size_t index, Probe probe) override;
    rs::Result<std::optional<balance::ForceExitHit>, std::string> force_exit_line(
        const BoardWorld& world, std::array<std::uint32_t, 2> actor, balance::ForceExitProbe probe) override;
};

}  // namespace s3::game::physics::grind_host
