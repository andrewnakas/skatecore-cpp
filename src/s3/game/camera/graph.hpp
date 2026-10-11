// Port of simref sim/src/camera/graph.rs (pub(super)).
//! Stock default camera graph, using the same recovered dynamic controller as
//! the action and motion graphs. Shot behavior side effects occur at Begin.
#pragma once
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "s3/core/camera/manager.hpp"
#include "s3/core/camera/slow_motion.hpp"
#include "s3/core/graph/controller.hpp"
#include "s3/data/vfs.hpp"
#include "s3/game/camera/graph_conditions.hpp"
#include "s3/game/camera/graph_subject.hpp"
#include "s3/game/camera/shot_data.hpp"
#include "s3/game/graph_runtime.hpp"

namespace s3::game::camera::graph {

/// Rust `enum Behavior` (private).
struct Behavior {
    struct Choose {
        std::vector<std::string> names;
        float incoming = 0.0f;
        float outgoing = 0.0f;
    };
    struct Print {
        std::string _0;
    };
    struct SlowMotion {};
    std::variant<Choose, Print, SlowMotion> value;
};

struct CameraGraph {
    // private in Rust
    s3::game::graph_runtime::CompiledGraph program;
    s3::graph::controller::Controller controller;
    std::vector<graph_conditions::Condition> conditions;
    std::vector<Behavior> behaviors;
    std::vector<std::optional<s3::camera::SlowMotionController>> slow_motion;
    s3::camera::SlowMotionSettings slow_motion_settings;

    /// Rust `load(path: &Path, ...)`; the graph file is read from `vfs` at `path`.
    static rs::Result<CameraGraph, std::string> load(const s3::data::Vfs& vfs, const std::string& path,
                                                     s3::camera::SlowMotionSettings slow_motion_settings);

    rs::Result<std::vector<s3::camera::SimulationRateRequest>, std::string> update(
        float dt, s3::camera::CameraMan& manager, const s3::camera::ManagerSubject& subject,
        const CameraGraphSubject& physical, const CameraGraphEnvironment& world,
        const shot_data::StockShots& database);
};

}  // namespace s3::game::camera::graph
