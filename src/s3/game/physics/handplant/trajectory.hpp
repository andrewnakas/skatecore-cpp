// Port of skate-game/src/physics/handplant/trajectory.rs
//! Handplant Launch82D613B0, trajectory selector82D66318/66948 and COM82D625C0.
//! (`impl Handplant` methods launch/select_outgoing/build_curve/values are declared in handplant.hpp.)
#pragma once
#include <array>

#include "s3/core/air/trajectory.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::handplant::trajectory {

using V = std::array<float, 4>;  // super::V
using ::s3::air::trajectory::Trajectory;

///82D61458/64 call standalone Cos/Sin. Permute mask822FBAC0 followed
///by vsldoi8 produces[sin,cos,0,0]: sine goes sideways, cosine goes up.
V apex_position(V coping, V side, float radius, float angle);
Trajectory to_apex(V start, V end);
Trajectory from_apex(V start, V end);
Trajectory arc_between(V start, V end, float time);
V bezier(std::array<V, 4> p, float t);

}  // namespace s3::game::physics::handplant::trajectory
