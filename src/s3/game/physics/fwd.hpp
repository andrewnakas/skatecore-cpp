// Forward declarations of the skate-game coordinator types, so module headers can declare
// `fn f(physics: &mut GamePhysics, skater: &mut SkaterRuntime)` without including
// physics.hpp / physics/skater.hpp (which include every module header).
#pragma once

namespace s3::game::physics {
struct GamePhysics;        // physics.hpp
struct SimulationExchange; // physics.hpp
namespace skater {
struct SkaterRuntime;      // physics/skater.hpp
}
using skater::SkaterRuntime;
namespace controls {
struct PlayerControls;     // physics/controls.hpp
}
using controls::PlayerControls;
}  // namespace s3::game::physics

namespace s3::game::camera {
namespace runtime {
struct CameraRuntime;      // camera/runtime.hpp (stage K), re-exported by camera.hpp
}
using runtime::CameraRuntime;
}
namespace s3::game::graph_runtime {
struct StockGraphs;        // graph_runtime.hpp (stage A)
}
namespace s3::game::skater_animation {
class SkaterAnimation;     // skater_animation.hpp (stage A)
}
