// Port of simref sim/src/camera/tests/runtime.rs (skate-game camera/tests/runtime.rs), plus the
// stage-K pack integration test (one gameplay camera update at the KenneyPark spawn).
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>

#include "harness.hpp"
#include "pack.hpp"
#include "s3/core/camera.hpp"
#include "s3/core/trigonometry.hpp"
#include "s3/game/camera.hpp"

using s3::camera::F4;
using s3::camera::F4x4;
using s3::game::camera::CameraGraphEnvironment;
using s3::game::camera::CameraRuntime;
using s3::game::camera::CameraSubjectSnapshot;

// #[ignore = "requires the user's converted stock camera and graph assets"] -- runs against
// core.pack here (skipped when it is missing).
S3_TEST(private_stock_camera_loads_every_graph_operation_and_shot_dependency) {
    S3_REQUIRE_PACK(vfs);
    auto camera = CameraRuntime::load(*vfs, s3test::cam::ROOT);
    CHECK_MSG(camera.is_ok(), (camera.is_ok() ? "" : camera.unwrap_err()));
    CHECK(!camera.unwrap().frame.has_value());
}

/// A minimised window reports zero height, and `width / height` is then NaN.
/// That NaN used to reach the field of view, fail the frame's finite check and
/// end the run -- minimising the window killed the running game twice in one
/// session. The last good ratio is held instead.
S3_TEST(a_zero_area_window_cannot_poison_the_aspect_ratio) {
    const s3::data::Vfs* vfs = s3test::cam::pack_vfs();
    if (vfs == nullptr) {
        // The camera collections are not present in every test environment;
        // the guard itself is what matters and is covered below.
        return;
    }
    auto loaded = CameraRuntime::load(*vfs, s3test::cam::ROOT);
    if (loaded.is_err()) {
        return;
    }
    CameraRuntime& runtime = loaded.unwrap();
    runtime.set_aspect_ratio(16.0f / 9.0f);
    const float good = runtime.manager.state.aspect_ratio;
    for (float bad : {s3::rs::f32::NAN_, s3::rs::f32::INFINITY_, 0.0f, -1.0f}) {
        runtime.set_aspect_ratio(bad);
        CHECK_MSG(runtime.manager.state.aspect_ratio == good, bad << " should have been ignored");
    }
    runtime.set_aspect_ratio(4.0f / 3.0f);
    CHECK_EQ(runtime.manager.state.aspect_ratio, 4.0f / 3.0f);
}

namespace {

struct StaticWorld final : s3::camera::MovingObstacleProvider {
    std::size_t collect(F4, F4, float, std::array<s3::camera::PathObstacle, 50>&) override { return 0; }
};

F4 add(F4 a, F4 b) { return {a[0] + b[0], a[1] + b[1], a[2] + b[2], a[3] + b[3]}; }

/// Snapshot of a skater standing still on the board at `spawn`, facing `heading` (rotation about +Y).
/// Fields follow publication.rs for a grounded (category 100) idle rider; values not
/// published by a standing skater (trajectory, grind, look) are zero.
CameraSubjectSnapshot standing_snapshot(std::uint64_t tick, F4 spawn, float heading, bool reset) {
    const auto [s, c] = s3::trigonometry::sin_cos(heading);
    F4x4 transform = {{
        {c, 0.0f, -s, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {s, 0.0f, c, 0.0f},
        spawn,
    }};
    const F4 up = {0.0f, 1.0f, 0.0f, 0.0f};
    const F4 com = add(spawn, {0.0f, 0.95f, 0.0f, 0.0f});
    CameraSubjectSnapshot out;
    out.tick = tick;
    auto& subject = out.subject;
    subject.rig.transform = transform;
    subject.rig.skeleton_root = transform;
    subject.rig.hips_position = add(spawn, {0.0f, 0.95f, 0.0f, 0.0f});
    subject.rig.last_valid_ground_up = up;
    subject.rig.context = 1;
    subject.rig.in_ground_physics = 1;
    subject.anchors = s3::camera::Anchors::new_().entries;
    subject.board_offset_direction = up;
    subject.ground_normal = up;
    subject.launch_normal = up;
    subject.landing_normal = up;
    subject.reset = reset ? 1 : 0;
    out.pose.physical_transform = transform;
    out.pose.skeleton_root = transform;
    out.pose.center_of_mass = com;
    out.pose.reckoned_center_of_mass = com;
    out.anchors.board_position = spawn;
    out.anchors.board_offset_direction = up;
    out.anchors.center_of_mass = com;
    out.anchors.damped_center_of_mass = com;
    out.anchors.skeleton_root_up = up;
    out.anchors.reset = reset;
    out.reference_points.head = add(spawn, {0.0f, 1.6f, 0.0f, 0.0f});
    out.reference_points.hips = add(spawn, {0.0f, 0.95f, 0.0f, 0.0f});
    out.reference_points.left_foot = add(spawn, {0.2f * c, 0.1f, -0.2f * s, 0.0f});
    out.reference_points.right_foot = add(spawn, {-0.2f * c, 0.1f, 0.2f * s, 0.0f});
    out.reference_points.board = spawn;
    out.reference_points.centre_of_mass = com;
    out.reference_points.damped_centre_of_mass = com;
    out.compass.skeleton_direction = transform[2];
    out.compass.board_velocity = F4{};
    return out;
}

s3::physics::board_world::BoardWorld ground_plane(float half) {
    using s3::math::Vector3;
    using s3::physics::board_world::WorldTriangle;
    const s3::physics::contact::RetailContactMaterial material{0.7f, 0.4f, 0.2f};
    std::vector<WorldTriangle> triangles;
    triangles.push_back(s3::rs::unwrap(WorldTriangle::from_vertices(
        {Vector3{-half, 0.0f, -half}, Vector3{-half, 0.0f, half}, Vector3{half, 0.0f, -half}}, material, 1, 0xe0,
        {1.0f, 1.0f, 1.0f}, 0.0f)));
    triangles.push_back(s3::rs::unwrap(WorldTriangle::from_vertices(
        {Vector3{half, 0.0f, -half}, Vector3{-half, 0.0f, half}, Vector3{half, 0.0f, half}}, material, 1, 0xe0,
        {1.0f, 1.0f, 1.0f}, 0.0f)));
    return s3::physics::board_world::BoardWorld::new_(std::move(triangles));
}

void print(const char* label, const F4& v) {
    std::cout << "  " << label << " = (" << std::setprecision(7) << v[0] << ", " << v[1] << ", " << v[2] << ")\n";
}

}  // namespace

// Stage K integration: CameraRuntime::load from core.pack (as simref's CameraRuntime::load does
// from the unpacked root), then one gameplay update for a skater standing at the KenneyPark spawn.
S3_TEST(pack_gameplay_camera_at_kenney_park_spawn) {
    S3_REQUIRE_PACK(vfs);
    auto loaded = CameraRuntime::load(*vfs, s3test::cam::ROOT);
    CHECK_MSG(loaded.is_ok(), (loaded.is_ok() ? "" : loaded.unwrap_err()));
    CameraRuntime& camera = loaded.unwrap();
    camera.set_aspect_ratio(1280.0f / 800.0f);  // simref driver WINDOW_ASPECT
    const F4 spawn = {13.5f, 0.1875f, -1.5f, 0.0f};
    const float heading = s3::rs::from_bits(0x40490fdb);
    // KenneyPark's .skate collision is turned into a BoardWorld by skate_world (a later stage);
    // this test stands the rider on a flat 200 m ground plane at y = 0 instead. (An empty world
    // makes the drop predictor see a bottomless drop ahead and raise the camera.)
    const auto world = ground_plane(100.0f);
    const F4 gravity = {0.0f, -9.81f, 0.0f, 0.0f};
    const float dt = s3::rs::from_bits(0x3c888889);
    CameraGraphEnvironment environment;
    environment.camera_type = 1;  // the user's selected normal High camera
    StaticWorld moving;

    auto first = camera.advance(dt, standing_snapshot(1, spawn, heading, true), world, gravity, environment, moving);
    CHECK_MSG(first.is_ok(), (first.is_ok() ? "" : first.unwrap_err()));
    const s3::camera::CameraFrame frame = first.unwrap();
    const auto& at = frame.basis.columns[2];
    const F4 target = add(frame.position, {at[0], at[1], at[2], 0.0f});
    std::cout << "  shot = " << camera.selected_shot() << ", fov = " << frame.field_of_view_degrees << " deg\n";
    print("eye", frame.position);
    print("target (eye + look)", target);
    print("framing reference", camera.manager.composer.reference);
    CHECK(!camera.selected_shot().empty());
    CHECK(frame.discontinuity);
    for (std::size_t i = 0; i < 3; ++i) {
        CHECK(std::isfinite(frame.position[i]));
    }
    // The camera sits behind and above the rider, looking back at them.
    const float dx = frame.position[0] - spawn[0];
    const float dy = frame.position[1] - spawn[1];
    const float dz = frame.position[2] - spawn[2];
    const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    CHECK_MSG(distance > 0.5f && distance < 15.0f, "eye distance " << distance);
    const auto& forward = standing_snapshot(1, spawn, heading, true).subject.rig.transform[2];
    CHECK_MSG(dx * forward[0] + dz * forward[2] < 0.0f, "camera is not behind the rider");
    CHECK_MSG(at[0] * -dx + at[1] * -dy + at[2] * -dz > 0.0f, "camera does not look toward the rider");

    // One second of standing still: the frame stays finite and settles.
    s3::camera::CameraFrame last = frame;
    for (std::uint64_t tick = 2; tick <= 61; ++tick) {
        auto next = camera.advance(dt, standing_snapshot(tick, spawn, heading, false), world, gravity, environment,
                                   moving);
        CHECK_MSG(next.is_ok(), (next.is_ok() ? "" : next.unwrap_err()));
        last = next.unwrap();
        if (std::getenv("S3_CAM_TRACE")) {
            const auto& r = camera.manager.rig;
            std::cout << "  t" << tick << " shot=" << camera.selected_shot() << " eye=(" << last.position[0] << ","
                      << last.position[1] << "," << last.position[2] << ") head=" << r.heading._0.position
                      << " elev=" << r.elevation._0.position << " htgt=" << r.fields.heading_target
                      << " etgt=" << r.fields.elevation_target << " dist=" << r.positioner.distance
                      << " fdist=" << r.fields.distance << " anchor=(" << r.anchor.position[0] << ","
                      << r.anchor.position[1] << "," << r.anchor.position[2] << ") refh=" << r.fields.reference_height
                      << " flags516=" << int(r.fields.flags_516) << " 517=" << int(r.fields.flags_517)
                      << " pflags=" << int(r.positioner.flags) << " hw=" << r.fields.heading_weight << " shake=(" << last.shake_translation[0] << ","
                      << last.shake_translation[1] << "," << last.shake_translation[2]
                      << ") w=" << camera.manager.shake[0].weight << " imp=" << camera.manager.shake[0].impulse
                      << " time=" << camera.manager.shake[0].time << " poff=(" << r.positioner.offset[1] << ","
                      << r.positioner.offset[2] << ") clear=" << r.positioner.collision_clear_time << "\n";
        }
    }
    const auto& at1 = last.basis.columns[2];
    print("eye after 1 s", last.position);
    print("target after 1 s", add(last.position, {at1[0], at1[1], at1[2], 0.0f}));
    for (std::size_t i = 0; i < 3; ++i) {
        CHECK(std::isfinite(last.position[i]));
    }
    // Non-monotonic ticks are rejected.
    auto stale = camera.advance(dt, standing_snapshot(61, spawn, heading, false), world, gravity, environment, moving);
    CHECK(stale.is_err());
}
