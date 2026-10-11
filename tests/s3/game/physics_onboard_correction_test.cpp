// Port of skate-game/src/physics/onboard_correction_tests.rs (mounted in physics/ground_phase.rs).
//! Real owner/helper integration with private stock construction. Synthetic
//! requests isolate latch semantics; these are not a recorded failure replay.
// The `#[ignore = "requires private stock skater, graph and collection assets"]` tests run on the
// cleanroom pack (skip without it).
#include <array>
#include <optional>
#include <utility>

#include "game_test_assets.hpp"
#include "harness.hpp"
#include "s3/core/physics/board_toolkit.hpp"
#include "s3/data/manifest.hpp"
#include "s3/game/graph_runtime.hpp"
#include "s3/game/physics.hpp"
#include "s3/game/physics/ground_phase.hpp"
#include "s3/game/physics/offboard/board_manager/runtime.hpp"
#include "s3/game/physics/skater.hpp"

using namespace s3::game;
using namespace s3::game::physics;

namespace {

std::optional<std::pair<GamePhysics, SkaterRuntime>> stock() {
    const auto* vfs = s3test::game::assets();
    if (vfs == nullptr) return std::nullopt;
    const std::string& root = s3test::game::ASSET_ROOT;
    auto assets = s3::data::GameAssets::load(*vfs, root);
    CHECK_MSG(assets.ok, assets.error);
    auto graphs = graph_runtime::StockGraphs::load(*vfs, root, assets.value).unwrap();
    auto physics = GamePhysics::load(*vfs, root).unwrap();
    auto skater = SkaterRuntime::load(*vfs, root, graphs, physics, "normal").unwrap();
    return std::make_pair(std::move(physics), std::move(skater));
}

template <class Body>
auto rates_of(const Body& body) {
    const auto& bodies = body.bodies();
    std::array<s3::physics::rigid_body::RetailBodyRates, std::tuple_size<std::decay_t<decltype(bodies)>>::value> out{};
    for (std::size_t i = 0; i < out.size(); ++i) out[i] = bodies[i].rates;
    return out;
}

}  // namespace

// #[ignore = "requires private stock skater, graph and collection assets"]
S3_TEST(canonical_correction_consumes_once_and_retains_suppressed_requests) {
    auto loaded = stock();
    if (!loaded) return;
    auto& skater = loaded->second;
    auto& body = skater.skeleton;
    auto& correction = skater.skeleton_output.correction;
    const std::array<float, 4> up{0.0f, 1.0f, 0.0f, 0.0f};
    correction.observe_board({0.25f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f});
    correction.pending = true;
    const auto before = body.record.pose;
    const auto rates = rates_of(body);
    correction.apply(body, up, false, 1u << 18, 0);
    CHECK(correction.pending);
    CHECK(body.record.pose == before);
    correction.apply(body, up, true, 0, 0);
    CHECK(correction.pending);
    CHECK(body.record.pose == before);
    correction.apply(body, up, false, 0, 0);
    CHECK(!correction.pending);
    CHECK(body.record.pose[0] == before[0]);
    for (std::size_t part = 1; part < 24; ++part) {
        CHECK_EQ(body.record.pose[part][3][0], before[part][3][0] + 0.25f);
    }
    CHECK(rates_of(body) == rates);
    const auto consumed = body.record.pose;
    // No producer write: a later tick cannot resurrect the consumed request.
    correction.apply(body, up, false, 0, 0);
    CHECK(body.record.pose == consumed);
    correction.pending = true;
    correction.observe_board({2.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f});
    correction.apply(body, up, true, 0, 1u << 30);
    CHECK_MSG(!correction.pending, "special wipeout clears even when too large to move");
    CHECK(body.record.pose == consumed);
}

// #[ignore = "requires private stock skater, graph and collection assets"]
S3_TEST(physical_pose_reset_clears_the_canonical_correction_request) {
    auto loaded = stock();
    if (!loaded) return;
    auto& physics = loaded->first;
    auto& skater = loaded->second;
    skater.skeleton_output.correction.pending = true;
    skeleton_input_runtime::SkeletonOwners owners{
        skater.animated_skeleton, skater.skeleton,       skater.skeleton_drives,          skater.foot_ik,
        skater.animation_input,   skater.skeleton_output.correction, skater.pose_errors,
    };
    skater.skeleton_input.reset_physical_pose(owners, physics.settings.step.simulation);
    CHECK(!skater.skeleton_output.correction.pending);
}

// #[ignore = "requires private stock skater, graph and collection assets"]
S3_TEST(ground_entry_restores_stock_materials_and_volumes_after_release) {
    auto loaded = stock();
    if (!loaded) return;
    auto& physics = loaded->first;
    auto& skater = loaded->second;
    const std::array<s3::physics::contact::RetailContactMaterial, 3> standard{
        physics.settings.standard_wheel_material,
        physics.settings.truck_material,
        physics.settings.deck_material,
    };
    {
        const auto& p = skater.player_input.processed;
        std::array<float, 4> normal{};
        for (std::size_t i = 0; i < 4; ++i) normal[i] = s3::rs::from_bits(p.vectors_464_480_496_512_528[0][i]);
        skater.player_input.toolkit = s3::physics::board_toolkit::BoardToolkit::from_board(
            physics.board, p.flags_2468, p.scalar_2612, normal, {0.0f, 1.0f, 0.0f, 0.0f});
    }
    {
        auto effects = skater.board_possession_live.effects(physics, skater.ground_lifecycle.board_animated_290,
                                                            skater.player_input.processed.timestep_2604);
        effects.released_board();
        effects.collision_volumes(false);
        effects.enable_animation_soft();
    }
    skater.board_possession_live.publish_volumes(physics);
    CHECK(physics.board_wiping_out);
    CHECK_EQ(physics.board.collision_group(), 7u);
    const auto entered = ground_phase::enter(physics, skater);
    CHECK_MSG(entered.is_ok(), (entered.is_ok() ? std::string() : entered.unwrap_err()));
    CHECK(!physics.board_wiping_out);
    CHECK_EQ(physics.board.collision_group(), 4u);
    const std::array<s3::physics::contact::RetailContactMaterial, 3> now{
        physics.settings.wheel_material,
        physics.settings.truck_material,
        physics.settings.deck_material,
    };
    CHECK(now == standard);
    const auto& volumes = skater.board_possession_live.volumes;
    CHECK(volumes.deck && volumes.trucks && volumes.wheels);
    bool all_children = true;
    for (const bool enabled : volumes.deck_children) all_children = all_children && enabled;
    CHECK(all_children);
    CHECK(physics.settings.truck_collisions);
    bool all_enabled = true;
    for (const auto& c : physics.settings.deck_geometry.children) all_enabled = all_enabled && c.collision_enabled;
    CHECK(all_enabled);
    CHECK_EQ(skater.ground_lifecycle.board_animated_290, std::uint8_t(0));
    const std::array<std::uint32_t, 8> dynamics{0, 0, 0, 2, 0, 0, 0, 2};
    CHECK(physics.board.hook().drive.dynamics == dynamics);
}
