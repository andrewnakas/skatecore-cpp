// Port of skate-core/src/player/offboard/ground_branch.rs (whole file).
#include "s3/core/player/offboard/ground_branch.hpp"

#include "s3/core/physics/native_arithmetic.hpp"
#include "s3/core/player/offboard/contact_correction.hpp"

namespace s3::player::offboard::ground_branch {

using contact_correction::inverse_length;
using contact_correction::magnitude;
using physics::native_arithmetic::dot3;

namespace {
const float DT = rs::from_bits(0x3c888889u);
}

bool select_alternate(const BranchInput& input) {
    const Vector velocity = input.movement_velocity_480[1] > input.output_velocity_512[1] ? input.output_velocity_512
                                                                                          : input.movement_velocity_480;
    if (input.suppressed_317 || (input.contact_flags_176 & 2u) == 0) return false;
    const Vector delta = rs::from_fn<4>([&](std::size_t i) { return input.contact_point_96[i] - input.position_48[i]; });
    if (dot3(delta, delta) <= rs::from_bits(0x3b23d70bu)) return false;
    // Native permuted multiply/subtract computes delta cross right.
    const Vector right = input.right_0;
    const Vector normal{rs::mul_add(-delta[2], right[1], delta[1] * right[2]),
                        rs::mul_add(-delta[0], right[2], delta[2] * right[0]),
                        rs::mul_add(-delta[1], right[0], delta[0] * right[1]),
                        rs::mul_add(-delta[3], right[3], delta[3] * right[3])};
    const float square = dot3(normal, normal);
    const float inverse = inverse_length(square);
    const Vector unit = magnitude(square) > rs::from_bits(0x358637bdu)
                            ? rs::map(normal, [&](float v) { return v * inverse; })
                            : Vector{};
    return dot3(velocity, unit) > 4.0f;
}

void integrate_alternate(Vector& velocity_480, float& speed_704, Vector& position_48,
                         position_output::FrameOutput& output, Vector forward_32, Vector frame_up_448,
                         Vector projection_axis_416) {
    // Original830BD330 initializer82F826B0 stores [0,-9.8,0,0], using
    // 82165A10 zero and822F8B40 C11CCCCD. It is not the splatted830BD400.
    const Vector gravity{0.0f, rs::from_bits(0xc11ccccdu), 0.0f, 0.0f};
    velocity_480 = rs::from_fn<4>([&](std::size_t i) { return rs::mul_add(gravity[i], DT, velocity_480[i]); });
    speed_704 = magnitude(dot3(velocity_480, velocity_480));
    position_48 = rs::from_fn<4>([&](std::size_t i) { return rs::mul_add(velocity_480[i], DT, position_48[i]); });
    output.update(forward_32, frame_up_448, projection_axis_416, position_48);
}

}  // namespace s3::player::offboard::ground_branch
