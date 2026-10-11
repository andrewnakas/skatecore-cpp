// Port of skate-core/src/physics/startup/reset.rs.
// Complete 82C0D680 and its 82C00D68 dependency, guest big-endian record bytes.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/physics/slice.hpp"

namespace s3::physics::startup::reset {

/// Complete CollisionInfo initialization/reset at 82C00D68. This is distinct
/// from the selective per-frame reset at 82C00CA0. Unwritten bytes survive.
/// (`info` points at the 812-byte record.)
void reset_collision_info(std::uint8_t* info);
inline void reset_collision_info(std::array<std::uint8_t, 812>& info) { reset_collision_info(info.data()); }

/// Complete SkateboardBody reset at 82C0D680. `bodies` are assembly parts in
/// their native order; each gravity vector is read from that body's Simulation
/// at +144. The hook assembly is separate and is not traversed here.
///
/// XYZ rates/torque clear; force XYZ receives the simulation vector. Fourth
/// lanes, pose, body flags/cooldown and all other body words survive. Ancillary
/// board bytes are preserved except for stores present in the native function.
/// Returns the original byte at board+8384, matching native r3.
std::uint8_t reset_board_body(std::array<std::uint8_t, 8400>& board, Slice<std::array<std::uint32_t, 44>> bodies,
                              Slice<const std::array<std::uint32_t, 4>> simulation_gravity);

}  // namespace s3::physics::startup::reset

namespace s3::physics::startup {
using reset::reset_board_body;
using reset::reset_collision_info;
}  // namespace s3::physics::startup
