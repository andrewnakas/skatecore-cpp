// Port of skate-core/src/physics/board_input_output.rs.
// Board-owned fields consumed by PhysicalPlayerHiLOD::Input, from original
// TU3 Skateboard::FillPhysOut82C02A80. Other physical components publish into
// the same packet after its native per-frame reset; their fields are retained.
#pragma once
#include <cstdint>

#include "s3/core/math.hpp"
#include "s3/core/physics/board_ground.hpp"
#include "s3/core/physics/board_motion_output.hpp"
#include "s3/core/player/input_phase.hpp"

namespace s3::physics::board_input_output {

using board_ground::BoardGroundState;
using board_motion_output::BoardMotionOutput;
using math::Vector3;
using player::input_phase::PhysicalPlayerInput;

/// Processed96 was captured by PrepareBoardToolkit before simulation. It is
/// deliberately separate from the current solved board transform. Ground64 is
/// the retained normal calculated by82C02388, not the wheel-contact normal.
struct BoardInputFrame {
    Vector3 processed_forward;
    Vector3 reckoning_normal_1216;
    Vector3 reckoning_ground_up;
    Vector3 retained_ground_normal_112;
    std::uint32_t processed_flags_2476 = 0;
};

void publish_board_input(PhysicalPlayerInput& out, const BoardMotionOutput& motion, const BoardGroundState& contacts,
                         BoardInputFrame frame);

/// Rust-private, exposed for the ported test.
/// 82C030B8..322C, including8286CD88's product gate and plane rejection.
float travel_angle(Vector3 forward, Vector3 velocity, Vector3 normal, Vector3 ground_up);

}  // namespace s3::physics::board_input_output
