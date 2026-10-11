// Port of skate-game/src/graph_host/motion_offboard/body_tweak/tests.rs (module `motion::body_tweak`).
#include <utility>
#include <vector>

#include "assets.hpp"
#include "harness.hpp"
#include "s3/core/animation/skeleton_input/name.hpp"
#include "s3/data/animation_banks.hpp"
#include "s3/game/animation_pose.hpp"
#include "s3/game/graph_host/motion/body_tweak.hpp"
#include "s3/game/graph_runtime/disk.hpp"

using namespace s3::game::graph_host::motion::body_tweak;
using s3::animation::output::Sqt;
using s3::animation::playback::PlaybackRequest;
using s3::animation::playback::TransitionSettings;
using s3::animation::playback_tree::Evaluation;
using s3::animation::playback_tree::PoseCommand;
using s3::animation::skeleton_input::name::encode;

namespace {
using Gate = std::pair<bool, bool>;
}

S3_TEST(native_air_tweak_release_latch_keeps_signed_axes_and_strict_counter) {
    State state;
    CHECK(state.gates(1.0f, 0.0f, false, 0.0f) == Gate(false, false));
    state.ticks = 80;
    CHECK(!state.gates(1.0f, 0.0f, false, 0.0f).first);
    state.ticks = 81;
    CHECK(state.gates(1.0f, 0.0f, false, 0.0f).first);
    state.begin();
    CHECK(state.gates(-1.0f, 0.0f, false, 0.0f).first);
    state.begin();
    CHECK(!state.gates(std::nullopt, 0.0f, false, 0.0f).first);
    CHECK_MSG(!state.released, "both native intent lookups must succeed to release");
    CHECK(state.gates(0.0f, 0.0f, false, 4.0f) == Gate(false, false));
    CHECK(state.gates(0.0f, 0.0f, false, 4.01f) == Gate(false, true));
    CHECK(state.gates(1.0f, 0.0f, true, 0.0f) == Gate(true, true));
}

S3_TEST(native_air_tweak_begin_preserves_filters_and_center_request_decays_them) {
    State state;
    state.filter(1.0f, -1.0f, false);
    CHECK(state.axes == (std::array<float, 2>{0.15f, -0.15f}));
    state.begin();
    CHECK(state.axes == (std::array<float, 2>{0.15f, -0.15f}));
    state.filter(-1.0f, 1.0f, true);
    CHECK(state.axes == (std::array<float, 2>{0.15f * 0.85f, -0.15f * 0.85f}));
    CHECK_EQ(state.ticks, 0u);
    CHECK(!state.released && !state.into_started && !state.cycle_started);
}

// #[ignore = "requires private stock OffBoard bank; graph leaf/pose regression, not gameplay acceptance"]
S3_TEST(stock_air_tweak_channels_parameters_pose_requests_and_teardown) {
    S3_REQUIRE_ASSETS(root);
    const s3::data::Vfs vfs = s3::game::graph_runtime::disk::read_banks(root);
    auto banks = s3::data::AnimationBanks::load(vfs, root);
    if (!banks.ok) s3test::fail(__FILE__, __LINE__, banks.error);
    auto evaluator_result = s3::game::animation_pose::PoseEvaluator::from_banks(banks.value);
    const auto& evaluator = evaluator_result.unwrap();
    const Settings settings{"B_OBAIR_BODYTWEAK_NB_CYC", "B_OBAIR_BODYTWEAK_NB_INTO", "B_OBAIR_BODYTWEAK_BR_CYC",
                            "B_OBAIR_BODYTWEAK_BR_INTO"};
    for (const bool held : {false, true}) {
        auto metadata = banks.value.metadata();
        if (!metadata.ok) s3test::fail(__FILE__, __LINE__, metadata.error);
        MotionAnimation animation = MotionAnimation::from_metadata(std::move(metadata.value));
        animation.skater_animation_flags = 0u;
        animation
            .play(PlaybackRequest{held ? "BR_STAND_0_CYC" : "NB_STAND_0_CYC", 1.0f, 0.0f,
                                  TransitionSettings{1, 0.0f, 0, 0, false}})
            .unwrap();
        Controls controls;
        State state;
        Physical physical{2.0f, 0.0f, held};
        std::vector<std::vector<Sqt>> poses;
        state.begin();
        animation.motion_intents.insert("OB_AirBodyTweakX", 0.0f);
        animation.motion_intents.insert("OB_AirBodyTweakY", 0.0f);
        state.update(settings, animation, controls, physical).unwrap();
        CHECK(!animation.channels.has(CHANNEL));
        const std::array<float, 2> directions[] = {{1.0f, 0.0f}, {-1.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, -1.0f}};
        for (const auto& direction : directions) {
            animation.motion_intents.insert("OB_AirBodyTweakX", direction[0]);
            animation.motion_intents.insert("OB_AirBodyTweakY", direction[1]);
            for (int i = 0; i < 90; ++i) {
                state.update(settings, animation, controls, physical).unwrap();
                CHECK(animation.pending_parameter(encode("bodytweakx")).has_value());
                CHECK(animation.pending_parameter(encode("bodytweaky")).has_value());
                CHECK_NE(*animation.skater_animation_flags & 0x8000u, 0u);
                CHECK(controls.seed_from_air_tweak);
                animation.apply_parameters().unwrap();
                animation.advance(1.0f / 60.0f, 0.0f);
                animation.refresh_tree_attributes().unwrap();
                const std::vector<PoseCommand> commands = animation.evaluate_pose(Evaluation{0.01f, true}).unwrap();
                const std::vector<Sqt> pose = evaluator.evaluate(commands).unwrap();
                for (const Sqt& s : pose) {
                    for (float v : s.rotation) CHECK(s3::rs::is_finite(v));
                    for (float v : s.translation) CHECK(s3::rs::is_finite(v));
                }
                bool channel_blend = false;
                for (const auto& c : commands) {
                    channel_blend = channel_blend || std::holds_alternative<PoseCommand::ChannelBlend>(c.value);
                }
                if (channel_blend) poses.push_back(pose);
            }
        }
        CHECK(state.into_started && state.cycle_started);
        bool changed = false;
        for (std::size_t i = 0; i + 1 < poses.size() && !changed; ++i) {
            for (std::size_t j = 0; j < poses[i].size() && j < poses[i + 1].size(); ++j) {
                if (poses[i][j].rotation != poses[i + 1][j].rotation ||
                    poses[i][j].translation != poses[i + 1][j].translation) {
                    changed = true;
                    break;
                }
            }
        }
        CHECK(changed);
        physical.time_to_land = 0.05f;
        animation.skater_animation_flags = 0u;
        state.update(settings, animation, controls, physical).unwrap();
        CHECK_NE(*animation.skater_animation_flags & 0x10000u, 0u);
        state.end(animation);
        for (int i = 0; i < 10; ++i) animation.advance(1.0f / 60.0f, 0.0f);
        CHECK(!animation.channels.has(CHANNEL));
        CHECK_MSG(controls.seed_from_air_tweak, "native End preserves Wipeout handoff latch");
    }
}
