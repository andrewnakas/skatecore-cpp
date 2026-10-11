// Port of the inline tests in skate-core/src/physics/phase.rs.
#include "harness.hpp"
#include "s3/core/physics/phase.hpp"

namespace {
using namespace s3::physics::phase;

S3_TEST(command_and_event_buffers_reject_stale_ticks) {
    auto commands = PhysicsCommandBuffer::new_(4);
    CHECK(commands.push(3, PhysicsCommand::RequestState{PhysicalStateId::PhysicsGround}).is_err());
    CHECK(commands.push(4, PhysicsCommand::RequestState{PhysicalStateId::PhysicsGround}).is_ok());
    auto events = PhysicsEventBuffer::new_(4);
    CHECK(events.emit(5, PhysicsEvent::Landing{}).is_err());
    CHECK(events.emit(4, PhysicsEvent::Landing{}).is_ok());
}
}  // namespace
