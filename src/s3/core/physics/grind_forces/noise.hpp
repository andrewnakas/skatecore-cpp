// Port of skate-core/src/physics/grind_forces/noise.rs.
// Original orientation noise82D40580 / S2 AddNoiseToOrientation82D8FE78.
// No replacement RNG: caller supplies the three consecutive board-stream u32
// draws, even for zero amount/speed. S3 hardcodes S2's full-speed/scalar tunables.
#pragma once
#include "s3/core/physics/grind_forces.hpp"

namespace s3::physics::grind_forces::noise {

std::array<float, 3> angles(float speed, float amount, std::array<std::uint32_t, 3> samples);

std::array<V, 4> apply(std::array<V, 4> frame, float speed, float amount,
                       std::array<std::uint32_t, 3> samples);

}  // namespace s3::physics::grind_forces::noise
