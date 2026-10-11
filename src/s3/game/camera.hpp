// Port of simref sim/src/camera.rs (Bevy-free skate-game camera): module tree + re-exports.
//! SIMREF: camera.rs reduced to its module tree + re-exports. Bevy CameraPlugin/spawn/present
//! (rendering endpoint) removed.
#pragma once
#include "s3/game/camera/collision.hpp"
#include "s3/game/camera/graph.hpp"
#include "s3/game/camera/graph_conditions.hpp"
#include "s3/game/camera/graph_subject.hpp"
#include "s3/game/camera/publication.hpp"
#include "s3/game/camera/runtime.hpp"
#include "s3/game/camera/settings.hpp"
#include "s3/game/camera/shake_data.hpp"
#include "s3/game/camera/shot_data.hpp"
#include "s3/game/camera/stock_names.hpp"
#include "s3/game/camera/subject.hpp"
#include "s3/game/camera/trajectory.hpp"
#include "s3/game/camera/world_query.hpp"

namespace s3::game::camera {
// pub(crate) use graph_subject::CameraGraphEnvironment;  (already in this namespace)
// pub(crate) use publication::{..., snapshot as publish_camera_subject};
template <class GamePhysics, class SkaterRuntime>
rs::Result<CameraSubjectSnapshot, std::string> publish_camera_subject(const GamePhysics& physics,
                                                                      const SkaterRuntime& skater,
                                                                      const CameraPublicationInputs& input) {
    return publication::snapshot(physics, skater, input);
}
// pub(crate) use runtime::CameraRuntime;
using runtime::CameraRuntime;
}  // namespace s3::game::camera
