// Port of skate-game/src/physics/offboard/air_selector/tests.rs
#include <cmath>
#include <tuple>

#include "harness.hpp"
#include "s3/game/physics/offboard/air_selector.hpp"

using namespace s3::game::physics::offboard::air_selector;
using s3::math::Vector3;
using s3::physics::board_world::WorldTriangle;
using s3::physics::board_world::query_metadata::Bounds;
using s3::physics::board_world::query_metadata::QueryMesh;
using s3::physics::board_world::query_metadata::QueryMetadata;
using s3::physics::board_world::query_metadata::QueryPool;
using s3::physics::contact::RetailContactMaterial;
using s3::physics::drive_frames::RetailAffineTransform;
using s3::player::offboard::air_selector::TrajectoryResult;
using s3::player::offboard::ground_query::Line;
using StaticScene = s3::game::physics::offboard::contact_toolkit::StaticScene;

namespace {
BoardWorld world(const std::vector<std::tuple<QueryPool, std::int32_t, std::uint16_t>>& pools,
                 std::uint32_t island_flags) {
    const std::array<Vector3, 3> vertices{Vector3::new_(-20.0f, 0.0f, -20.0f), Vector3::new_(-20.0f, 0.0f, 20.0f),
                                          Vector3::new_(20.0f, 0.0f, 0.0f)};
    const RetailContactMaterial material{0.6f, 0.4f, 0.1f};
    std::vector<WorldTriangle> triangles;
    std::vector<QueryMesh> meshes;
    QueryMetadata metadata{};
    for (std::size_t i = 0; i < pools.size(); ++i) {
        triangles.push_back(*WorldTriangle::from_vertices(vertices, material, 999, 0, {0.0f, 0.0f, 0.0f}, 0.0f));
        QueryMesh m{};
        m.triangle_range = {i, i + 1};
        m.local_to_world = RetailAffineTransform::IDENTITY;
        m.world_to_local = RetailAffineTransform::IDENTITY;
        m.local_bounds = *Bounds::from_points(vertices);
        m.matching_group = std::get<1>(pools[i]);
        m.rejection_flags = 0x6000;
        m.geometry = static_cast<std::uint32_t>(i);
        m.pool = std::get<0>(pools[i]);
        metadata.meshes.push_back(m);
        metadata.packed_surfaces.push_back(std::get<2>(pools[i]));
    }
    metadata.island_flags = island_flags;
    return BoardWorld::with_query_metadata(std::move(triangles), std::move(metadata)).unwrap();
}
Context context() { return Context{0, -1, {0.0f, 1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 0.0f}}; }
AirSelector owner() {
    Settings s{};
    s.deck_center_to_truck = 0.3f;
    s.query = core::Settings{0.9f, 0.3f, 0};
    s.blend.x.fill(0.0f);
    s.blend.y.fill(1.0f);
    return AirSelector::new_(s);
}
Packet packet() {
    Packet p = Packet::initialized(0.0f);
    p.position_32 = {0.0f, 2.0f, 0.0f, 0.0f};
    p.velocity_0 = {0.0f, 0.0f, 3.0f, 0.0f};
    p.forward_64 = {0.0f, 0.0f, 1.0f, 0.0f};
    return p;
}
const Vector GRAVITY{0.0f, -9.8f, 0.0f, 0.0f};
}  // namespace

S3_TEST(six_real_lines_keep_native_pool_ties_and_exact_surfaces) {
    const auto w = world({{QueryPool::Ground, 2, 0x411}, {QueryPool::Island, 2, 0x722}}, 0);
    const auto scene = StaticScene::new_(w).unwrap();
    const Line line{Vector3::new_(0.0f, 1.0f, 0.0f), Vector3::new_(0.0f, -1.0f, 0.0f), 0.0f};
    const std::vector<Line> lines(6, line);
    const auto hits = scene.lines(lines, 2).unwrap();
    CHECK_EQ(hits.size(), std::size_t(6));
    for (const auto& hit : hits) {
        CHECK(hit.has_value());
        CHECK_EQ(hit->packed_surface, std::uint16_t(0x411));
        CHECK(std::fabs(hit->fraction - 0.5f) < 1e-5f);
    }
    for (const auto& hit : scene.lines(lines, 3).unwrap()) CHECK(!hit.has_value());
    //Mesh0x6000 does not reject these lines; unlike trajectory their mask is0.
}

S3_TEST(conditional_pool_requires_exact_island_three) {
    const Line line{Vector3::new_(0.0f, 1.0f, 0.0f), Vector3::new_(0.0f, -1.0f, 0.0f), 0.0f};
    for (std::uint32_t flags : {0u, 1u, 2u, 3u, 7u}) {
        const auto w = world({{QueryPool::Conditional, -1, 0x845}}, flags);
        const auto result = StaticScene::new_(w).unwrap().lines({line}, -1).unwrap();
        CHECK_EQ(result[0].has_value(), flags == 3);
    }
}

S3_TEST(line_rejects_nonfinite_input_without_synthesizing_a_hit) {
    const auto w = world({{QueryPool::Ground, -1, 1}}, 0);
    const auto scene = StaticScene::new_(w).unwrap();
    const Line line{Vector3::new_(s3::rs::f32::NAN_, 1.0f, 0.0f), Vector3::ZERO, 0.0f};
    CHECK(scene.lines({line}, -1).is_err());
    CHECK(scene.lines({}, -1).unwrap().empty());
}

S3_TEST(synchronous_results_remain_staged_until_native_consume) {
    const auto w = world({{QueryPool::Ground, -1, 1}}, 0);
    auto o = owner();
    TrajectoryResult out{};
    CHECK(o.launch(w, packet(), GRAVITY, context()).is_ok());
    CHECK(o.core.sampling.pending_8492);
    o.sample(0, core::DT, out);
    CHECK(!out.valid_404);
    CHECK(out.position_272 == packet().position_32);
    CHECK((o.consume(w, context(), 0.4f).unwrap() == std::optional<std::size_t>(0)));
    CHECK(!o.core.sampling.pending_8492);
    CHECK(o.core.sampling.preinitialized_8494);
    //The actual mask0x6000 rejects the plane in trajectory requests.
    CHECK(!o.core.selected_candidate.valid_120);
    CHECK(o.requery(w, context(), 0, 0).unwrap());
    CHECK(o.core.requery_pending_8499);
    CHECK(o.consume(w, context(), 0.4f).is_ok());
    CHECK(!o.core.requery_pending_8499);
    CHECK(o.core.sampling.restart_allowed_8493);
    CHECK(!o.core.sampling.preinitialized_8494);
}

S3_TEST(failed_world_submission_preserves_owner_and_never_returns_fake_completion) {
    const auto w = BoardWorld::new_({});
    auto o = owner();
    CHECK(o.launch(w, packet(), GRAVITY, context()).is_err());
    CHECK(!o.core.sampling.pending_8492);
    CHECK(!o.completed_launch.has_value());
    CHECK(!o.completed_requery.has_value());
}

S3_TEST(launch_and_requery_share_slot_zero_and_consume_in_native_order) {
    const auto w = world({{QueryPool::Ground, -1, 1}}, 0);
    auto o = owner();
    CHECK(o.launch(w, packet(), GRAVITY, context()).is_ok());
    CHECK(o.requery(w, context(), 0, 0).unwrap());
    CHECK(o.core.sampling.pending_8492 && o.core.requery_pending_8499);
    CHECK(o.completed_launch.has_value() && o.completed_requery.has_value());
    CHECK(!o.requery(w, context(), 0, 0).unwrap());
    CHECK((o.consume(w, context(), 0.3f).unwrap() == std::optional<std::size_t>(0)));
    CHECK(!o.core.sampling.pending_8492 && !o.core.requery_pending_8499);
    CHECK(!o.completed_launch.has_value() && !o.completed_requery.has_value());
    CHECK(o.core.sampling.restart_allowed_8493);
}

S3_TEST(native_suppression_returns_before_pending_host_completion) {
    const auto w = world({{QueryPool::Ground, -1, 1}}, 0);
    auto o = owner();
    CHECK(o.launch(w, packet(), GRAVITY, context()).is_ok());
    CHECK(!o.requery(w, context(), 0x10000000, 0).unwrap());
    CHECK(!o.requery(w, context(), 0, 0x400000).unwrap());
    CHECK(o.core.sampling.pending_8492);
}
