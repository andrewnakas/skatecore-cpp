// Port of skate-game/src/physics/grind_host.rs.
//! Bind the manager to the current board materials and authored world queries.
#include "s3/game/physics/grind_host.hpp"

#include "s3/game/physics/player_input/grind/world.hpp"

namespace s3::game::physics::grind_host {

Status LiveHost::apply_material_mode(MaterialMode mode) {
    materials.apply(mode, board, settings);
    return ok();
}

rs::Result<std::optional<ProbeHit>, std::string> LiveHost::surface_probe(const BoardWorld& world,
                                                                         std::array<std::uint32_t, 2> actor,
                                                                         std::size_t index, Probe probe) {
    return player_input::grind::world::surface_probe(world, actor, index, probe);
}

rs::Result<std::optional<balance::ForceExitHit>, std::string> LiveHost::force_exit_line(
    const BoardWorld& world, std::array<std::uint32_t, 2> actor, balance::ForceExitProbe probe) {
    return player_input::grind::world::force_exit_line(world, actor, probe);
}

}  // namespace s3::game::physics::grind_host
