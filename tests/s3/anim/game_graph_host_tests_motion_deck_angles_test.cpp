// Port of skate-game/src/graph_host/tests/motion_deck_angles.rs (the `deck_angles_tests` module of
// motion.rs).
#include "assets.hpp"
#include "harness.hpp"
#include "motion_host_fixture.hpp"
#include "s3/core/player/input_phase.hpp"

using namespace s3::game::graph_host::motion;
namespace fixture = s3test::motion_fixture;
using s3::animation::skeleton_input::name::encode;

// #[ignore = "requires private stock graphs and animation banks"]
S3_TEST(stock_deck_angles_copy_completed_output_only_on_begin) {
    S3_REQUIRE_ASSETS(root);
    const auto graph = fixture::stock_graph(root);
    MotionHost host = fixture::host(root, graph, fixture::context(false, false, true));
    std::size_t behavior = host.remap.behaviors.size();
    for (std::size_t i = 0; i < host.remap.behaviors.size(); ++i) {
        if (host.operations[host.remap.behaviors[i]].is<MotionOperation::SetDeckPitchAndYaw>()) {
            behavior = i;
            break;
        }
    }
    CHECK_MSG(behavior < host.remap.behaviors.size(), "stock graph must contain the reported behavior");
    const std::size_t op = host.remap.behaviors[behavior];
    CHECK(host.operations[op] ==
          MotionOperation(MotionOperation::SetDeckPitchAndYaw{encode("skateyaw"), encode("skatepitch")}));
    const auto frame = fixture::frame(1.0f / 60.0f);
    const std::uint32_t handle = host.allocate(behavior, frame);
    CHECK(host.execute(behavior, frame, 0).is_err());
    s3::player::input_phase::SkeletonOutputFields output{};
    output.publish_deck_angles({1.0f, 0.5f, 1.0f, 0.0f}, true);
    const std::array<float, 2> values = {output.deck_yaw_536, output.deck_pitch_540};
    host.deck_yaw_pitch = values;
    host.execute(behavior, frame, 0).unwrap();
    CHECK_EQ(host.animation.pending_parameter(encode("skateyaw")), std::optional<float>(values[0]));
    CHECK_EQ(host.animation.pending_parameter(encode("skatepitch")), std::optional<float>(values[1]));
    host.deck_yaw_pitch = std::array<float, 2>{9.0f, 8.0f};
    host.execute(behavior, frame, 1).unwrap();
    host.execute(behavior, frame, 2).unwrap();
    CHECK_EQ(host.animation.pending_parameter(encode("skateyaw")), std::optional<float>(values[0]));
    CHECK_EQ(host.animation.pending_parameter(encode("skatepitch")), std::optional<float>(values[1]));
    host.release(handle);
    host.allocate(behavior, frame);
    host.execute(behavior, frame, 0).unwrap();
    CHECK_EQ(host.animation.pending_parameter(encode("skateyaw")), std::optional<float>(9.0f));
    CHECK_EQ(host.animation.pending_parameter(encode("skatepitch")), std::optional<float>(8.0f));
}
