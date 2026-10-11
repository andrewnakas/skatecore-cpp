// Port of skate-core/src/animation/foot_ik/two_bone.rs.
// Math::_TwoBoneIKSolve8296F8C0, TU3 complete reachable/recursive/invalid path.
// Plane construction uses the native general inverse, not a rigid transpose.
#pragma once
#include <cstdint>

#include "s3/core/animation/foot_ik/math.hpp"

namespace s3::animation::foot_ik::two_bone {

using math::Vector;

enum class SolveResult {
    Solved,
    Extended,
    Invalid,
};

struct AngleLimits {
    float minimum_degrees = 0.0f;
    float maximum_degrees = 0.0f;
};

SolveResult solve(Vector root, Vector middle, Vector end, Vector& solved_middle, Vector& target,
                  AngleLimits limits, bool override_maximum_with_original, std::uint32_t recursion_budget);

}  // namespace s3::animation::foot_ik::two_bone
