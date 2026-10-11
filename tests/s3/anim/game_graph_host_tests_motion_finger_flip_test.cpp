// Port of skate-game/src/graph_host/tests/motion_finger_flip.rs (the `finger_flip_tests` module of
// motion.rs).
#include "assets.hpp"
#include "harness.hpp"
#include "motion_host_fixture.hpp"

using namespace s3::game::graph_host::motion;
namespace fixture = s3test::motion_fixture;
using s3::animation::skeleton_input::name::encode;

// #[ignore = "requires private stock graphs and animation banks"]
S3_TEST(stock_finger_flip_crash_node_3080_lifecycle_and_intent_source) {
    using Operation = s3::game::graph_host::motion_stock_gameplay::Operation;
    S3_REQUIRE_ASSETS(root);
    const auto graph = fixture::stock_graph(root);
    MotionHost host = fixture::host(root, graph, fixture::context(false, false, true));
    const std::size_t behavior = 3080;
    const auto* gameplay =
        std::get_if<MotionOperation::StockGameplay>(&host.operations[host.remap.behaviors.at(behavior)].value);
    const Operation::FingerFlipOut* finger =
        gameplay ? std::get_if<Operation::FingerFlipOut>(&gameplay->_0.value) : nullptr;
    if (finger == nullptr) S3_PANIC("The reported stock behavior must bind FingerFlipOut");
    const std::string intent = finger->grab_intent;
    CHECK_MSG(!intent.empty() && intent.find('$') == std::string::npos, "authored intent must be resolved");
    const auto frame = fixture::frame(0.125f);
    host.allocate(behavior, frame);
    host.execute(behavior, frame, 0).unwrap();
    CHECK_EQ(host.animation.pending_parameter(encode("Grabbing")), std::optional<float>());

    // Raw AG input cannot substitute for the MG intent read by virtual16.
    host.action_intents.insert(intent, 1.0f);
    host.execute(behavior, frame, 1).unwrap();
    const float released = host.animation.pending_parameter(encode("Grabbing")).value();
    CHECK(released >= 0.0f && released < 1.0f);
    // Native membership test treats zero as present and reverses the timer.
    host.animation.motion_intents.insert(intent, 0.0f);
    host.execute(behavior, frame, 1).unwrap();
    CHECK_EQ(host.animation.pending_parameter(encode("Grabbing")), std::optional<float>(1.0f));
    host.animation.motion_intents.remove(intent);
    for (int i = 0; i < 6; ++i) host.execute(behavior, frame, 1).unwrap();
    CHECK_EQ(host.animation.pending_parameter(encode("Grabbing")), std::optional<float>(0.0f));
    host.execute(behavior, frame, 2).unwrap();
    CHECK_EQ(host.animation.pending_parameter(encode("Grabbing")), std::optional<float>(0.0f));
    // Reentry resets private time, but Begin/End do not publish a parameter.
    host.execute(behavior, frame, 0).unwrap();
    CHECK_EQ(host.animation.pending_parameter(encode("Grabbing")), std::optional<float>(0.0f));
    host.execute(behavior, frame, 1).unwrap();
    CHECK_EQ(host.animation.pending_parameter(encode("Grabbing")), std::optional<float>(released));

    std::size_t second = host.remap.behaviors.size();
    for (std::size_t id = 0; id < host.remap.behaviors.size(); ++id) {
        if (id == behavior) continue;
        const auto* g = std::get_if<MotionOperation::StockGameplay>(&host.operations[host.remap.behaviors[id]].value);
        if (g != nullptr && g->_0.is<Operation::FingerFlipOut>()) {
            second = id;
            break;
        }
    }
    CHECK_MSG(second < host.remap.behaviors.size(), "stock graph has separate finger-flip exit nodes");
    host.allocate(second, frame);
    host.execute(second, frame, 0).unwrap();
    host.execute(second, frame, 1).unwrap();
    CHECK_EQ(host.animation.pending_parameter(encode("Grabbing")), std::optional<float>(released));
    host.execute(behavior, frame, 1).unwrap();
    CHECK(host.animation.pending_parameter(encode("Grabbing")).value() < released);
    CHECK_MSG(host.animation.motion_attributes.empty(), "writes a tree parameter, not a physics packet");
}
