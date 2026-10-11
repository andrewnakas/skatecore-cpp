// Port of skate-game/src/physics/known_air/math.rs.
//! Original KnownAir VMX geometry; independent PC estimate seeds.
//! (`impl KnownAirMath for Runtime<'_>` is declared in known_air/runtime.hpp and defined in math.cpp.)
#pragma once
#include <array>
#include <utility>

#include "s3/core/math.hpp"

namespace s3::game::physics::known_air::math {

using V = std::array<float, 4>;

float dot(V a, V b);
V sub(V a, V b);
V scale(V a, float s);
V cross(V a, V b);
std::pair<V, float> normalized(V v);
V rotate(const std::array<V, 4>& m, V v);
V point(const std::array<V, 4>& m, V v);
::s3::math::Vector3 xyz(V v);

}  // namespace s3::game::physics::known_air::math
