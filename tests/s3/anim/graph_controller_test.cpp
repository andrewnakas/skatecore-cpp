// Port of the #[cfg(test)] module in skate-core/src/graph/controller.rs.
#include <string>

#include "harness.hpp"
#include "s3/core/graph/controller.hpp"

using namespace s3::graph::controller;
using s3::graph::activation::ConditionId;

namespace {
struct HostLog : Host {
    std::vector<std::string> _0;
    std::uint32_t condition_activation(ConditionId, const Frame&) override { S3_PANIC("unreachable"); }
    std::array<std::uint32_t, 6> context() const override { return {1, 2, 0, 4, 5, 6}; }
    std::uint32_t allocate(BehaviorId, const Frame&) override { S3_PANIC("unreachable"); }
    void begin(BehaviorId, std::array<std::uint32_t, 6>, const Frame&) override { S3_PANIC("unreachable"); }
    void update(BehaviorId, std::array<std::uint32_t, 6>, const Frame&) override { S3_PANIC("unreachable"); }
    void end(BehaviorId behavior, std::array<std::uint32_t, 6> context, const Frame&) override {
        _0.push_back("end:" + std::to_string(behavior) + ":" + std::to_string(context[2]));
    }
    void hook(HookId, const Frame&) override { S3_PANIC("unreachable"); }
    void release(std::uint32_t instance) override { _0.push_back("release:" + std::to_string(instance)); }
};
}  // namespace

S3_TEST(end_all_behaviors_uses_reverse_order_and_instance_context) {
    auto controller = Controller::new_(0);
    controller.active = {ActiveBehavior{4, 40}, ActiveBehavior{7, 70}};
    HostLog host;
    controller.end_all_behaviors(host);
    CHECK_EQ(host._0, (std::vector<std::string>{"end:7:70", "release:70", "end:4:40", "release:40"}));
    CHECK(controller.active.empty());
}
