// skater_animation.rs has no #[test] of its own. This C++-only smoke test (no Rust counterpart)
// drives the stock end-to-end path the Rust driver uses: StockGraphs + Collections + both banks
// from the asset root -> SkaterAnimation::load -> evaluate_initial_pose -> advance.
#include <fstream>
#include <iostream>
#include <sstream>

#include "assets.hpp"
#include "harness.hpp"
#include "s3/data/manifest.hpp"
#include "s3/game/graph_runtime/disk.hpp"
#include "s3/game/skater_animation.hpp"

using namespace s3::game::skater_animation;
namespace disk = s3::game::graph_runtime::disk;

namespace {

s3::data::GameAssets manifest(const std::string& root) {
    std::ifstream file(s3::data::join_path(root, disk::MANIFEST), std::ios::binary);
    std::stringstream text;
    text << file.rdbuf();
    auto parsed = s3::data::GameAssets::parse(text.str());
    if (!parsed.ok) s3test::fail(__FILE__, __LINE__, parsed.error);
    return parsed.value;
}

AnimationPhysical riding_physical() {
    AnimationPhysical p{};
    s3::graph::conditions::SpeedInputs speeds{};
    speeds.speed = 2.0f;
    speeds.forward_speed = 2.0f;
    speeds.speed_and_slope = 2.0f;
    p.conditions.speeds = speeds;
    s3::graph::conditions::PhysicalStateInputs state{};
    state.category = 1;
    p.conditions.physical_state = state;
    p.conditions.time_since_last_input = 0.0f;
    p.conditions.mirrored = false;
    p.conditions.riding_fakie = false;
    p.conditions.push_brake = s3::graph::conditions::PushBrakeInputs{};
    p.foot_frame.deck_y = {0.0f, 1.0f, 0.0f, 0.0f};
    p.foot_frame.deck_z = {0.0f, 0.0f, 1.0f, 0.0f};
    p.board_present = true;
    p.time_since_teleport = 1.0f;
    return p;
}

/// Stand-ins for the completed physical publications the driver (physics/animation_phase.rs)
/// writes before each advance. Zero-valued, but present, so graph leaves can execute.
void publish_defaults(SkaterAnimation& animation) {
    namespace gh = s3::game::graph_host;
    auto& motion = animation.motion;
    motion.score_packet = gh::motion_native::ScorePacket{};
    motion.manual_exit = false;
    motion.deck_yaw_pitch = std::array<float, 2>{0.0f, 0.0f};
    motion.offboard_cadence_phase = 0.0f;
    motion.offboard_locomotion_state = 0u;
    motion.ground_slope_type = 0u;
    motion.biped_ground_thin = false;
    animation.action.dropping_in = false;
    motion.toggle_board_physical = gh::motion_toggle_board::Physical{};
    motion.runout_physical = gh::motion_runout::Observation{};
    motion.wipeout_physical = gh::motion_wipeout::Physical{};
    motion.landing_physical = gh::motion_landing::Physical{};
    motion.prelanding_physical = gh::motion_spin::PrelandingPhysical{};
    motion.air_leg_physical = s3::animation::air_leg_extension::Physical{};
    animation.action.physical_conditions = gh::action::PhysicalConditions{};
    animation.action.physical_conditions->state = 100;  // PhysicsGround
    motion.gameplay_conditions = gh::motion_gameplay_conditions::GameplayConditions{};
    motion.gameplay_conditions->state = 100;  // PhysicsGround
    motion.gameplay_conditions->time_to_skitch = -1.0f;  // no skitch target
    animation.action.gameplay_conditions = motion.gameplay_conditions;
    motion.native_physical = gh::motion_native::Physical{};
    motion.bump_acceleration = std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f};
    motion.gesture_physical = gh::motion_native::GesturePhysical{};
    motion.shove_physical = gh::motion_shove::ShovePhysical{};
    motion.riding_conditions = gh::motion_riding_conditions::RidingConditionInputs{};
}

}  // namespace

S3_TEST(stock_skater_animation_loads_and_evaluates_initial_pose) {
    S3_REQUIRE_ASSETS(root);
    const s3::data::GameAssets assets = manifest(root);
    const s3::data::Vfs graph_files = disk::read_files(root, {assets.action_graph, assets.motion_graph});
    const StockGraphs graphs = StockGraphs::load(graph_files, root, assets).unwrap();
    const s3::data::Vfs collection_files = disk::read_files(root, {disk::COLLECTIONS});
    auto collections = s3::data::Collections::load(collection_files, root);
    if (!collections.ok) s3test::fail(__FILE__, __LINE__, collections.error);
    auto loaded = SkaterAnimation::load(root, collections.value, graphs, "");
    if (loaded.is_err()) s3test::fail(__FILE__, __LINE__, loaded.unwrap_err());
    SkaterAnimation animation = std::move(loaded.unwrap());
    CHECK_EQ(animation.packet.bone_count, static_cast<std::uint32_t>(animation.evaluator->frames.bone_names.size()));
    CHECK_EQ(animation.ticks, std::uint64_t(0));
    const auto initial = animation.evaluate_initial_pose().unwrap();
    CHECK_EQ(initial.size(), animation.evaluator->frames.bone_names.size());
    CHECK_EQ(animation.pose.size(), initial.size());
    // Shared source: a second skater reuses the decoded banks.
    auto second = SkaterAnimation::from_source(collections.value, graphs, "Loose", animation.source);
    CHECK(second.is_ok());

    s3::animation::output::packet_reset::AdditionalResetFields reset{};
    s3::graph::intents::IntentMap intents;
    for (int tick = 0; tick < 5; ++tick) {
        publish_defaults(animation);
        auto advanced = animation.advance(graphs, 1.0f / 60.0f, intents, riding_physical(), reset);
        // Graph leaves report a missing completed physical publication as an error (as in Rust).
        if (advanced.is_err()) s3test::fail(__FILE__, __LINE__, advanced.unwrap_err());
        CHECK_EQ(animation.ticks, std::uint64_t(tick + 1));
        CHECK_EQ(animation.pose.size(), initial.size());
        CHECK_EQ(animation.packet.hierarchy.size(), initial.size());
    }
    std::cout << "SkaterAnimation advanced " << animation.ticks << " ticks; current tree "
              << animation.motion.animation.current_name.value_or("<none>") << "\n";
}
