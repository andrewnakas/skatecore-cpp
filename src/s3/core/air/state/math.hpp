// Port of skate-core/src/air/state/math.rs.
// Independent arithmetic for original PhysicsAir's jump-velocity helpers.
// Retains original refinements; PC results are not certified Xenon bit-exact.
#pragma once
#include <array>

#include "s3/core/air/state/jump_velocity.hpp"

namespace s3::air::state::math {

using jump_velocity::PhysicsAirMath;

/// Unit struct implementing PhysicsAirMath (`AirMath.f(..)` -> `AirMath{}.f(..)`).
class AirMath final : public PhysicsAirMath {
public:
    float minimum_vminfp(float left, float right) override;
    float length_squared_vmsum3fp(std::array<float, 4> value) override;
    float length_vmsum3fp_vrsqrte(std::array<float, 4> value) override;
    std::array<float, 4> clamp_vector_within_max_length(std::array<float, 4> value, float maximum) override;
};

/// 8296EBB0 uses one normalization refinement and a clamped dot product.
float angle_between_vectors(std::array<float, 4> left, std::array<float, 4> right);

/// Toolkit_ClampJumpVelocity82D93518. The output uses the CURRENT velocity's
/// direction; the supplied reference only bounds its magnitude.
std::array<float, 4> clamp_jump_velocity(std::array<float, 4> reference, std::array<float, 4> current);

}  // namespace s3::air::state::math
