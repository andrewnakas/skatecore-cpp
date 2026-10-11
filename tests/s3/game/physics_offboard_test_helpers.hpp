// Shared helpers for the pack-backed playback tests (ports of skate-game/src/tests/*_playback.rs):
//   - `edge_world`: tests/offboard_air_playback.rs `pub(super) fn edge_world`, also used by
//     offboard_midair_playback.rs and offboard_recall_playback.rs.
//   - `Playback`: the per-test asset/runtime setup every playback test repeats inline in Rust
//     (GameAssets/StockGraphs/GamePhysics/SkaterRuntime/PlayerControls/ControllerInput/CameraRuntime).
//   - `joint_gap_failure`: the identical anchor-gap loops of offboard_midair (assert_physical_joints),
//     offboard_jump (assert_joints) and offboard_recall (assert_joint_anchors).
//   - finiteness predicates shared by the playback assertions.
// tests/offboard_root_trace.rs is not ported: it is only called from frame.rs under #[cfg(test)]
// (the C++ frame.cpp has no such hook) and only prints when SKATE3_TRACE_OFFBOARD_ROOT is set.
#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "game_test_assets.hpp"
#include "harness.hpp"
#include "s3/core/input/xbox.hpp"
#include "s3/core/physics/board_world.hpp"
#include "s3/core/physics/board_world/query_metadata.hpp"
#include "s3/core/physics/collision.hpp"
#include "s3/core/physics/drive_frames.hpp"
#include "s3/core/player/state.hpp"
#include "s3/data/manifest.hpp"
#include "s3/game/camera.hpp"
#include "s3/game/graph_runtime.hpp"
#include "s3/game/input/controllers.hpp"
#include "s3/game/physics.hpp"
#include "s3/game/physics/controls.hpp"
#include "s3/game/physics/frame.hpp"
#include "s3/game/physics/ground.hpp"
#include "s3/game/physics/skater.hpp"

namespace s3test::game::offboard_playback {

using s3::game::physics::GamePhysics;
using s3::game::physics::PlayerControls;
using s3::game::physics::SkaterRuntime;
using s3::input::xbox::XboxState;
using s3::math::Vector3;
using s3::physics::board_world::BoardWorld;
using s3::physics::board_world::WorldTriangle;
using s3::player::state::PhysicalStateId;

/// Rust `crate::physics::PHYSICS_MODE` (physics.rs: `pub(crate) const PHYSICS_MODE: &str = "easy"`).
inline const char* const PHYSICS_MODE = "easy";

inline XboxState xbox(std::uint16_t buttons, std::array<std::uint8_t, 2> triggers, std::array<std::int16_t, 2> left,
                      std::array<std::int16_t, 2> right) {
    XboxState state{};
    state.buttons = buttons;
    state.triggers = triggers;
    state.left = left;
    state.right = right;
    return state;
}

/// Test-authored collision geometry, not imported render triangles or a
/// support override: four triangles queried by the real world/analyzer.
inline BoardWorld edge_world(s3::physics::contact::RetailContactMaterial material, float extent, float floor_y) {
    std::vector<WorldTriangle> triangles;
    struct Level {
        float extent;
        float height;
        std::uint32_t tag;
    };
    for (const Level level : {Level{extent, s3::game::physics::ground::HEIGHT, 1u}, Level{50.0f, floor_y, 0u}}) {
        const std::array<Vector3, 4> corners{
            Vector3{-level.extent, level.height, -level.extent},
            Vector3{level.extent, level.height, -level.extent},
            Vector3{level.extent, level.height, level.extent},
            Vector3{-level.extent, level.height, level.extent},
        };
        for (const std::array<int, 3> indices : {std::array<int, 3>{0, 2, 1}, std::array<int, 3>{0, 3, 2}}) {
            auto triangle = WorldTriangle::from_vertices(
                {corners[indices[0]], corners[indices[1]], corners[indices[2]]}, material, level.tag,
                s3::physics::collision::TriangleFeature::ONE_SIDED, {1.0f, 1.0f, 1.0f}, 0.0f);
            triangles.push_back(s3::rs::unwrap(triangle));
        }
    }
    namespace qm = s3::physics::board_world::query_metadata;
    // Explicit test-level collision metadata, using the same authoring contract
    // as Terrain::Flat. No synthetic support/contact publication.
    std::vector<Vector3> points;
    for (const auto& t : triangles)
        for (const auto& v : t.triangle.vertices) points.push_back(v);
    qm::QueryMetadata metadata;
    metadata.packed_surfaces = std::vector<std::uint16_t>(triangles.size(), 0);
    qm::QueryMesh mesh;
    mesh.triangle_range = qm::Range{0, triangles.size()};
    mesh.local_to_world = s3::physics::drive_frames::RetailAffineTransform::IDENTITY;
    mesh.world_to_local = s3::physics::drive_frames::RetailAffineTransform::IDENTITY;
    mesh.local_bounds = s3::rs::unwrap(qm::Bounds::from_points(points));
    mesh.matching_group = -1;
    mesh.rejection_flags = 0;
    mesh.geometry = 1;
    mesh.pool = qm::QueryPool::Ground;
    metadata.meshes = {mesh};
    metadata.static_edges = {};
    metadata.island_flags = 0;
    return BoardWorld::with_query_metadata(std::move(triangles), std::move(metadata)).unwrap();
}

/// The common per-test runtime (heap-allocated: the runtimes are large). `load` returns
/// nullptr (skip) without the pack.
struct Playback {
    const s3::data::Vfs* vfs = nullptr;
    s3::game::graph_runtime::StockGraphs graphs;
    GamePhysics physics;
    SkaterRuntime skater;
    PlayerControls controls;
    s3::game::input::controllers::ControllerInput input{};
    s3::game::camera::CameraRuntime camera;

    /// `stock_controls`: Rust `PlayerControls::load(root)` instead of `PlayerControls::default()`.
    /// `world`: applied to `physics.world` before `SkaterRuntime::load` (as the Rust tests do).
    template <class WorldFn>
    static std::unique_ptr<Playback> load_with(const std::string& mode, bool stock_controls, WorldFn world) {
        const auto* vfs = assets();
        if (vfs == nullptr) return nullptr;
        const std::string& root = ASSET_ROOT;
        auto manifest = s3::data::GameAssets::load(*vfs, root);
        if (!manifest.ok) s3test::fail(__FILE__, __LINE__, "GameAssets::load: " + manifest.error);
        auto graphs = s3::game::graph_runtime::StockGraphs::load(*vfs, root, manifest.value).unwrap();
        auto physics = GamePhysics::load(*vfs, root).unwrap();
        world(physics);
        auto skater = SkaterRuntime::load(*vfs, root, graphs, physics, mode).unwrap();
        auto camera = s3::game::camera::CameraRuntime::load(*vfs, root).unwrap();
        PlayerControls controls = stock_controls ? PlayerControls::load(*vfs, root).unwrap() : PlayerControls{};
        return std::unique_ptr<Playback>(new Playback{vfs,
                                                      std::move(graphs),
                                                      std::move(physics),
                                                      std::move(skater),
                                                      std::move(controls),
                                                      s3::game::input::controllers::ControllerInput{},
                                                      std::move(camera)});
    }
    static std::unique_ptr<Playback> load(const std::string& mode, bool stock_controls) {
        return load_with(mode, stock_controls, [](GamePhysics&) {});
    }

    /// `controls.update(...)` + `controls.publish_gestures(...)` path (onboard playback tests).
    auto sample_update(const XboxState& raw, bool gestures) {
        input.sample_raw_for_test(raw);
        auto actions = input.player_actions();
        controls.update(actions, physics.settings.step.simulation.time_step, physics.settings.input_magnitude_threshold,
                        skater.player_input.physical.scoring.capabilities_204);
        if (gestures) {
            controls.publish_gestures(physics.animation_profile.physics_mode,
                                      skater.player_input.physical.state.state_16);
        }
        return actions;
    }
    /// `controls.update_for_physics(...).unwrap()` path (offboard playback tests).
    auto sample_for_physics(const XboxState& raw) {
        input.sample_raw_for_test(raw);
        auto actions = input.player_actions();
        controls.update_for_physics(actions, physics, skater, camera).unwrap();
        return actions;
    }
    s3::game::Status advance(s3::input::controller::ActionMap& actions) {
        return s3::game::physics::frame::advance(physics, skater, controls, graphs, actions, true, camera);
    }
};

inline bool render_pose_finite(const SkaterRuntime& skater) {
    for (const auto& m : skater.render_pose)
        for (const auto& row : m)
            for (float v : row)
                if (!std::isfinite(v)) return false;
    return true;
}

/// `camera.frame.as_ref().expect("completed camera").position.iter().all(is_finite)`.
inline bool camera_position_finite(const s3::game::camera::CameraRuntime& camera) {
    if (!camera.frame) s3test::fail(__FILE__, __LINE__, "completed camera");
    for (float v : camera.frame->position)
        if (!std::isfinite(v)) return false;
    return true;
}

inline float gap_sum3(const std::array<float, 3>& a, const std::array<float, 3>& b) {
    // Rust `(0..3).map(|i| (a[i]-b[i]).powi(2)).sum::<f32>().sqrt()`; sum starts at -0.0.
    float sum = -0.0f;
    for (int i = 0; i < 3; ++i) {
        const float d = a[i] - b[i];
        sum += d * d;
    }
    return std::sqrt(sum);
}

/// The joint-anchor gap loop shared by the offboard playback tests. Returns the first failing
/// joint as (parent, child, gap), Rust panics on the first `gap < 0.35` failure.
struct JointGap {
    std::size_t parent;
    std::size_t child;
    float gap;
};
inline std::optional<JointGap> joint_gap_failure(const SkaterRuntime& skater) {
    const auto& bodies = skater.skeleton.bodies();
    auto anchor = [&](std::size_t part, const std::uint32_t* words) {
        const auto& body = bodies[part];
        std::array<float, 3> local{};
        for (int i = 0; i < 3; ++i) local[i] = s3::rs::from_bits(words[i]);
        const auto basis = body.rates.basis.columns;
        const auto p = body.rates.position;
        const std::array<float, 3> position{p.x, p.y, p.z};
        std::array<float, 3> out{};
        for (int i = 0; i < 3; ++i) {
            out[i] = position[i] + basis[0][i] * local[0] + basis[1][i] * local[1] + basis[2][i] * local[2];
        }
        return out;
    };
    for (const auto& joint : skater.skeleton_joints.records) {
        const auto child = anchor(joint.child, joint.frames.words.data() + 4);
        const auto parent = anchor(joint.parent, joint.frames.words.data() + 12);
        const float gap = gap_sum3(child, parent);
        if (!(gap < 0.35f)) return JointGap{joint.parent, joint.child, gap};
    }
    return std::nullopt;
}

inline bool hand_dynamics_released(const std::array<std::array<std::uint32_t, 4>, 2>& dynamics) {
    const std::array<std::uint32_t, 4> idle{0, 0, 0, 2};
    return dynamics[0] == idle && dynamics[1] == idle;
}

}  // namespace s3test::game::offboard_playback
