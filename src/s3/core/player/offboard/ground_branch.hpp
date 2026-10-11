// Port of skate-core/src/player/offboard/ground_branch.rs (whole file).
// Original82D7D868 branch selector and82D7E1A8 alternate integration.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/player/offboard/position_output.hpp"

namespace s3::player::offboard::ground_branch {

using Vector = std::array<float, 4>;

struct BranchInput {
    Vector movement_velocity_480{};
    Vector output_velocity_512{};
    Vector right_0{};
    Vector position_48{};
    Vector contact_point_96{};
    std::uint32_t contact_flags_176 = 0;
    bool suppressed_317 = false;
};
/// Recomputed Biped709. This selects a ground-controller branch, not a physical state.
bool select_alternate(const BranchInput& input);
/// 82D7E1A8 writes480/704/48 and then calls82D80360. The caller skips
/// 82D80548 and cadence on this branch, but still publishes82D80F48 afterward.
void integrate_alternate(Vector& velocity_480, float& speed_704, Vector& position_48,
                         position_output::FrameOutput& output, Vector forward_32, Vector frame_up_448,
                         Vector projection_axis_416);

}  // namespace s3::player::offboard::ground_branch
