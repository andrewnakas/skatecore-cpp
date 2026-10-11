// Port of skate-game/src/physics/player_input/grind/world.rs.
//! Static host for original S3 82C20728 and 82E0AAD0 line batches.
//! Both use scene mask7, matching ID2952, side flags3, rejection mask0 and
//! a null surface-exclusion table. Metadata is mandatory, never render tags.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "s3/core/air/trajectory/grind_surface.hpp"
#include "s3/core/math.hpp"
#include "s3/core/physics/board_world.hpp"
#include "s3/core/physics/grind_contact/balance.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::player_input::grind::world {

using sc::air::trajectory::grind_surface::Probe;
using sc::air::trajectory::grind_surface::ProbeHit;
using sc::physics::board_world::BoardWorld;
using sc::physics::grind_contact::balance::ForceExitHit;
using sc::physics::grind_contact::balance::ForceExitProbe;

///82C205D8/82C20728: descriptor5 has radius .001; the others have radius0.
/// The descriptor supplies its radius, so both thin and swept leaves execute.
rs::Result<std::optional<ProbeHit>, std::string> surface_probe(const BoardWorld& world,
                                                               std::array<std::uint32_t, 2> actor, std::size_t index,
                                                               Probe probe);

///82D86C88 passes processed2952 into82E0AAD0, not actor identity2948.
rs::Result<std::optional<ForceExitHit>, std::string> force_exit_line(const BoardWorld& world,
                                                                     std::array<std::uint32_t, 2> actor,
                                                                     ForceExitProbe probe);

/// Private in Rust (exposed for world_tests / grind tests).
rs::Result<std::optional<ProbeHit>, std::string> line(const BoardWorld& world, std::int32_t matching, Probe probe);
sc::math::Vector3 vector(std::array<float, 4> v);
std::array<float, 4> packed(sc::math::Vector3 v);
sc::math::Vector3 sub(sc::math::Vector3 a, sc::math::Vector3 b);
//827719B8's ordered fsel choices, including the unordered upper choice.
float clamp_fraction(float fraction);
//82ACA4E0..550, from decoded authored winding, with two rsqrt refinements.
//As in current core native_arithmetic, the PC seed is sqrt().recip(); this
//preserves host arithmetic policy, not bit-exact Xenon estimate emulation.
sc::math::Vector3 face(std::array<sc::math::Vector3, 3> vertices);

}  // namespace s3::game::physics::player_input::grind::world
