// Port of simref sim/src/camera/runtime.rs.
//! Complete normal camera owner: subject publications, stock graph, CameraMan,
//! swept world queries and retained query history. The simulation calls advance
//! once per completed physical/animation publication; rendering only presents it.
#pragma once
#include <array>
#include <optional>
#include <string>
#include <vector>

#include "s3/core/camera/manager.hpp"
#include "s3/core/camera/shake.hpp"
#include "s3/core/camera/slow_motion.hpp"
#include "s3/core/physics/board_world.hpp"
#include "s3/data/vfs.hpp"
#include "s3/game/camera/graph.hpp"
#include "s3/game/camera/graph_subject.hpp"
#include "s3/game/camera/shot_data.hpp"
#include "s3/game/camera/subject.hpp"
#include "s3/game/camera/trajectory.hpp"

namespace s3::game::camera::runtime {

struct CameraRuntime {
    // private in Rust (pub here so tests and hosts can inspect them, like the Rust in-crate tests)
    s3::camera::CameraMan manager;
    subject::SubjectPublisher subject;
    graph::CameraGraph graph;
    shot_data::StockShots shots;
    s3::camera::ManagerSettings settings;
    s3::camera::CompassSettings compass_settings;
    std::array<s3::camera::ShakeSamples, 2> shakes;
    std::array<trajectory::TrajectoryResult, 3> trajectories;
    // pub in Rust:
    std::optional<s3::camera::CameraFrame> frame;
    /// Immutable physical subject publication consumed by the camera graph.
    /// Rendering may inspect this snapshot for diagnostics without rebuilding
    /// subject fields from mutable skater state.
    std::optional<CameraSubjectSnapshot> latest_subject;
    /// Native message order, including End followed by Begin in one graph tick.
    /// The simulation schedule drains these after advance.
    std::vector<s3::camera::SimulationRateRequest> simulation_rate_requests;

    /// Rust `load(root: &Path)`: `root` is the asset root inside `vfs` (e.g. "assets" for core.pack).
    static rs::Result<CameraRuntime, std::string> load(const s3::data::Vfs& vfs, const std::string& root);

    /// Ignores a ratio that is not a positive, finite number.
    void set_aspect_ratio(float value);
    const std::string& selected_shot() const { return this->manager.shots.current().name; }

    rs::Result<s3::camera::CameraFrame, std::string> advance(float dt, const CameraSubjectSnapshot& snapshot,
                                                             const s3::physics::board_world::BoardWorld& world,
                                                             s3::camera::F4 query_gravity,
                                                             const CameraGraphEnvironment& environment,
                                                             s3::camera::MovingObstacleProvider& moving);
};

}  // namespace s3::game::camera::runtime
