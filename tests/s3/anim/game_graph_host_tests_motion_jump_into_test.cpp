// Port of skate-game/src/graph_host/tests/motion_jump_into.rs (the `jump_into_tests` module of motion.rs).
#include <cmath>

#include "assets.hpp"
#include "harness.hpp"
#include "motion_host_fixture.hpp"
#include "s3/core/animation/playback.hpp"
#include "s3/core/animation/playback_tree.hpp"

using namespace s3::game::graph_host::motion;
namespace fixture = s3test::motion_fixture;
using s3::animation::skeleton_input::name::encode;

// #[ignore = "requires private stock graphs and animation banks"]
S3_TEST(stock_jump_into_seeks_once_per_entry_and_consumes_misses) {
    using s3::animation::playback::PlaybackRequest;
    using s3::animation::playback::TransitionSettings;
    namespace stock = s3::game::graph_host::motion_stock_gameplay;
    S3_REQUIRE_ASSETS(root);
    const auto graph = fixture::stock_graph(root);
    MotionHost host = fixture::host(root, graph, fixture::context(false, false, true));
    host.animation.skater_animation_flags = 0u;
    std::size_t behavior = host.remap.behaviors.size();
    for (std::size_t i = 0; i < host.remap.behaviors.size(); ++i) {
        const MotionOperation& op = host.operations[host.remap.behaviors[i]];
        if (const auto* gameplay = std::get_if<MotionOperation::StockGameplay>(&op.value)) {
            if (const auto* jump = std::get_if<stock::Operation::JumpInto>(&gameplay->_0.value)) {
                if (jump->attribute == encode("manualInto")) {
                    behavior = i;
                    break;
                }
            }
        }
    }
    CHECK(behavior < host.remap.behaviors.size());
    const auto frame = fixture::frame(1.0f / 60.0f);
    PlaybackRequest request;
    request.animation = "B_HARDFLIP_G";
    request.speed = 1.0f;
    request.start_time = 0.0f;
    request.transition = TransitionSettings{1, 0.0f, 0, 0, false};
    const float expected = host.animation.build_tree("B_HARDFLIP_G")
                               .unwrap()
                               .attribute(encode("manualInto"), 31)
                               .unwrap()
                               .value()
                               .begin_time;
    CHECK_MSG(std::isfinite(expected), "stock manualInto marker must have a finite begin time");
    for (int pass = 0; pass < 2; ++pass) {
        host.animation.play(request).unwrap();
        const std::uint32_t handle = host.allocate(behavior, frame);
        host.execute(behavior, frame, 0).unwrap();
        CHECK_EQ(host.animation.current_time().unwrap(), 0.0f);
        host.execute(behavior, frame, 1).unwrap();
        CHECK_EQ(host.animation.current_time().unwrap(), expected);
        host.animation.advance(frame.dt, 0.0f);
        const float advanced = host.animation.current_time().unwrap();
        host.execute(behavior, frame, 1).unwrap();
        CHECK_EQ(host.animation.current_time().unwrap(), advanced);
        host.execute(behavior, frame, 2).unwrap();
        host.release(handle);
        CHECK(host.animation.motion_attributes.empty());
    }
    // A first-update miss also consumes the native instance latch.
    const std::size_t op = host.remap.behaviors[behavior];
    host.operations[op] = MotionOperation::StockGameplay{stock::Operation::JumpInto{encode("missing-marker")}};
    host.animation.play(request).unwrap();
    host.allocate(behavior, frame);
    host.execute(behavior, frame, 1).unwrap();
    const auto* latch = host.instances[behavior].get<Instance::JumpInto>();
    CHECK(latch != nullptr && !latch->first_update);
    host.operations[op] = MotionOperation::StockGameplay{stock::Operation::JumpInto{encode("manualInto")}};
    host.execute(behavior, frame, 1).unwrap();
    CHECK_EQ(host.animation.current_time().unwrap(), 0.0f);
}
