// Port of skate-game/src/physics/grind/forces.rs.
//! Original force leaves wired to CURRENT's immediate deck accumulator.
#pragma once
#include <array>

#include "s3/core/physics/board_runtime.hpp"
#include "s3/core/physics/grind_forces.hpp"
#include "s3/core/physics/grind_forces/release.hpp"
#include "s3/core/physics/grind_forces/slide.hpp"
#include "s3/core/point_graph.hpp"
#include "s3/game/physics/grind/observation.hpp"

namespace s3::game::physics::grind::forces {

using observation::ManagerObservation;
using V = std::array<float, 4>;
using ::s3::physics::board_runtime::BoardRuntime;

///82D3FD88. The family owns its three strengths and position/velocity snapshot.
void friction(BoardRuntime& board, const ManagerObservation& manager, V position, V velocity, V normal,
              std::array<float, 3> strengths);

void lateral_pin(BoardRuntime& board, const ManagerObservation& manager, std::array<V, 4> frame, V across, V normal,
                 V velocity, float strength, float forward_offset, float up_offset,
                 const ::s3::point_graph::PointGraph<4>& pin_vs_slope);

void slide(BoardRuntime& board, ::s3::physics::grind_forces::slide::Slide family,
           ::s3::physics::grind_forces::slide::Input input);

void release(BoardRuntime& board, ::s3::physics::grind_forces::release::Input input);

}  // namespace s3::game::physics::grind::forces
