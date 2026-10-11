// Port of skate-game/src/physics/ground_runtime/corrections.rs.
//! Native numerical callbacks for the ordinary ground-state branch.
//! (All items are `impl GroundRuntime` methods / trait impls, declared in
//! GroundRuntime in ground_runtime.hpp; bodies live in corrections.cpp.)
#pragma once
#include <array>

#include "s3/core/math.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::ground_runtime::corrections {

/// Private helper.
sc::math::Vector3 xyz(std::array<float, 4> v);

}  // namespace s3::game::physics::ground_runtime::corrections
