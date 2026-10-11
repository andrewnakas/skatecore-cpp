// Port of skate-game/src/physics/frame.rs.
//! One production tick: controller graphs, physical input/state, shared solve,
//! physical animation output and the normal gameplay camera.
#pragma once
#include "s3/core/input/controller.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/fwd.hpp"

namespace s3::game::physics::frame {

Status advance(GamePhysics& physics, SkaterRuntime& skater, PlayerControls& controls,
               const graph_runtime::StockGraphs& graphs, sc::input::controller::ActionMap& actions,
               bool input_available, camera::CameraRuntime& camera);

}  // namespace s3::game::physics::frame
