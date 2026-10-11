// Port of the test in skate-game/src/physics/wipeout_states/skeleton.rs (`mod tests`).
// `#[ignore = "requires private stock skater and collection assets"]`: runs on the cleanroom pack
// (skips without it).
#include <array>

#include "game_test_assets.hpp"
#include "harness.hpp"
#include "s3/data/manifest.hpp"
#include "s3/game/graph_runtime.hpp"
#include "s3/game/physics.hpp"
#include "s3/game/physics/skater.hpp"
#include "s3/game/physics/wipeout_states/skeleton.hpp"

using namespace s3::game;
using namespace s3::game::physics;
using s3::math::Vector3;
using s3::physics::skeleton_animation_record::AnimationPartTransform;
using s3::physics::skeleton_animation_record::IDENTITY;

S3_TEST(retained_velocity_skips_all_wipeout_target_writes) {
    const auto* vfs = s3test::game::assets();
    if (vfs == nullptr) return;
    const std::string& root = s3test::game::ASSET_ROOT;
    auto assets = s3::data::GameAssets::load(*vfs, root);
    CHECK_MSG(assets.ok, assets.error);
    auto graphs = graph_runtime::StockGraphs::load(*vfs, root, assets.value).unwrap();
    auto physics = GamePhysics::load(*vfs, root).unwrap();
    auto skater = SkaterRuntime::load(*vfs, root, graphs, physics, "normal").unwrap();
    skater.player_input.processed.state_2508 = 300;
    skater.player_input.processed.state_timer_2664 = 1.0f;
    // Distinct synthetic animation target exposes accidental substitution
    // of the real physical parts; no world-space body is moved here.
    skater.skeleton_input.drive_frames.fill(IDENTITY);
    skater.skeleton_input.drive_frames[17][3][0] = 0.25f;
    for (const std::size_t part : {std::size_t(24), std::size_t(25)}) {
        skater.skeleton.bodies_mut()[part].rates.linear_velocity = Vector3{1.25f, -2.5f, 3.75f};
    }
    const std::array<Vector3, 2> velocities{skater.skeleton.bodies()[24].rates.linear_velocity,
                                            skater.skeleton.bodies()[25].rates.linear_velocity};
    std::array<AnimationPartTransform, 4> targets{};
    for (std::size_t i = 0; i < 4; ++i) targets[i] = skater.skeleton_drives.targets.transform(i);
    const auto positions = skater.skeleton_input.extra_target_positions;
    const auto board_target = skater.skeleton_input.animation_board_to_physics;
    skeleton_input_runtime::SkeletonOwners owners{
        skater.animated_skeleton, skater.skeleton,       skater.skeleton_drives,          skater.foot_ik,
        skater.animation_input,   skater.skeleton_output.correction, skater.pose_errors,
    };
    const auto& hierarchy = skater.animation.packet.hierarchy;
    auto updated = wipeout_states::skeleton::update(skater.skeleton_input, owners, skater.player_input.processed,
                                                    hierarchy.data(), hierarchy.size(), skater.wipeout_state.drives,
                                                    0.0f, 0.0f, 0.0f, 0.0f, true);
    CHECK_MSG(updated.is_ok(), (updated.is_ok() ? std::string() : updated.unwrap_err()));
    const std::array<Vector3, 2> after{owners.body.bodies()[24].rates.linear_velocity,
                                       owners.body.bodies()[25].rates.linear_velocity};
    CHECK(after == velocities);
    std::array<AnimationPartTransform, 4> after_targets{};
    for (std::size_t i = 0; i < 4; ++i) after_targets[i] = owners.drives.targets.transform(i);
    CHECK(after_targets == targets);
    CHECK(skater.skeleton_input.extra_target_positions == positions);
    CHECK(skater.skeleton_input.animation_board_to_physics == board_target);
    CHECK(owners.drives.bones[17].has_value());
    const auto& knee = *owners.drives.bones[17];
    for (std::size_t channel = 0; channel < 2; ++channel) {
        if (knee.active[channel]) {
            CHECK_MSG(knee.frames[channel].body_b.translation.x == 0.25f,
                      "UpdateRagdoll must consume the animation-volume buffer12624");
        }
    }
}
