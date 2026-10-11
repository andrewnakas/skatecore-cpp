// Port of skate-game/src/graph_host/tests/motion_board_adjust.rs (the `board_adjust_tests` module of
// motion.rs).
#include <cmath>
#include <functional>

#include "assets.hpp"
#include "harness.hpp"
#include "motion_host_fixture.hpp"
#include "s3/core/animation/playback_tree.hpp"

using namespace s3::game::graph_host::motion;
namespace fixture = s3test::motion_fixture;

// #[ignore = "requires private stock graphs and animation banks"]
S3_TEST(board_adjust_stock_release_keeps_the_authored_air_blend) {
    using s3::animation::playback_tree::Evaluation;
    using s3::animation::playback_tree::PoseCommand;
    S3_REQUIRE_ASSETS(root);
    const auto graph = fixture::stock_graph(root);
    MotionHost host = fixture::host(root, graph, fixture::context(false, false, true));
    host.animation.skater_animation_flags = 0u;
    auto find = [&](const std::function<bool(const MotionOperation&)>& predicate) {
        for (std::size_t i = 0; i < host.remap.behaviors.size(); ++i) {
            if (predicate(host.operations[host.remap.behaviors[i]])) return i;
        }
        S3_PANIC("called `Option::unwrap()` on a `None` value");
    };
    const std::size_t adjust = find([](const MotionOperation& op) {
        const auto* p = std::get_if<MotionOperation::Play>(&op.value);
        return p != nullptr && p->_0.animation == "BLEND_ADJUST_RIGHT";
    });
    const std::size_t air = find([](const MotionOperation& op) {
        const auto* p = std::get_if<MotionOperation::Play>(&op.value);
        return p != nullptr && p->_0.animation == "B_AIR_CYC" && p->_0.transition.kind == 2 &&
               p->_0.transition.under == 1;
    });
    std::array<std::size_t, 2> filters{};
    const char* filter_names[2] = {"RightBoardAdjustMag", "RightBoardAdjustAngle"};
    for (std::size_t n = 0; n < 2; ++n) {
        const std::string name = filter_names[n];
        filters[n] = find([&](const MotionOperation& op) {
            const auto* p = std::get_if<MotionOperation::IntentFilter>(&op.value);
            return p != nullptr && p->_0.intent == name && p->_0.settings.blend_out == std::optional<float>(0.08f);
        });
    }
    const auto frame = fixture::frame(1.0f / 60.0f);
    for (const std::size_t id : filters) {
        host.allocate(id, frame);
        host.execute(id, frame, 0).unwrap();
    }
    host.allocate(adjust, frame);
    host.execute(adjust, frame, 0).unwrap();
    for (int i = 0; i < 60; ++i) {
        host.animation.advance(frame.dt, 0.0f);
        host.animation.motion_intents.insert("RightBoardAdjustMag", 0.9f);
        host.animation.motion_intents.insert("RightBoardAdjustAngle", 0.6f);
        for (const std::size_t id : filters) host.execute(id, frame, 1).unwrap();
        host.execute(adjust, frame, 1).unwrap();
        host.animation.apply_parameters().unwrap();
    }
    host.animation.motion_intents.remove("RightBoardAdjustMag");
    host.animation.motion_intents.remove("RightBoardAdjustAngle");
    bool reached_exit = false;
    for (int i = 0; i < 120; ++i) {
        host.animation.advance(frame.dt, 0.0f);
        for (const std::size_t id : filters) host.execute(id, frame, 1).unwrap();
        host.execute(adjust, frame, 1).unwrap();
        host.animation.apply_parameters().unwrap();
        bool all = true;
        for (const char* name : {"BoardAdjustMag", "BoardAdjustAngle"}) {
            const float* value = host.animation.filtered_intents.get(name);
            if (value == nullptr) S3_PANIC("called `Option::unwrap()` on a `None` value");
            all = all && std::fabs(*value) < 0.4f;
        }
        if (all) {
            reached_exit = true;
            break;
        }
    }
    CHECK(reached_exit);
    host.animation.refresh_tree_attributes().unwrap();
    host.execute(adjust, frame, 2).unwrap();
    for (const std::size_t id : filters) host.execute(id, frame, 2).unwrap();
    host.allocate(air, frame);
    host.execute(air, frame, 0).unwrap();
    host.animation.apply_parameters().unwrap();
    Evaluation evaluation{};
    evaluation.cull_threshold = 0.0f;
    evaluation.update_history = false;
    auto last_blend = [](const std::vector<PoseCommand>& commands) -> const PoseCommand::Blend* {
        return commands.empty() ? nullptr : std::get_if<PoseCommand::Blend>(&commands.back().value);
    };
    {
        const std::vector<PoseCommand> commands = host.animation.evaluate_pose(evaluation).unwrap();
        const auto* blend = last_blend(commands);
        CHECK_MSG(blend != nullptr && blend->weight == 0.0f, "new air tree must not replace the outgoing pose");
    }
    float previous = 0.0f;
    for (int i = 0; i < 12; ++i) {
        host.animation.advance(frame.dt, 0.0f);
        const std::vector<PoseCommand> commands = host.animation.evaluate_pose(evaluation).unwrap();
        const auto* blend = last_blend(commands);
        if (blend == nullptr) S3_PANIC("missing authored 0.2s blend");
        CHECK(blend->weight >= previous && blend->weight <= 1.0f);
        previous = blend->weight;
    }
    CHECK(previous > 0.999f);
}
