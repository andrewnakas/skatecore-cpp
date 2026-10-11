// Port of skate-game/src/physics.rs #[cfg(test)] mod exchange_tests.
#include "harness.hpp"
#include "s3/game/physics.hpp"

using namespace s3::game::physics;
using s3::physics::phase::PhysicsEvent;
using s3::player::state::PhysicalStateId;

S3_TEST(exchange_keeps_events_on_the_authoritative_tick) {
    auto exchange = SimulationExchange::new_(8);
    CHECK(exchange
              .emit_event(7, PhysicsEvent{PhysicsEvent::StateChanged{PhysicalStateId::PhysicsGround,
                                                                     PhysicalStateId::PhysicsAir}})
              .is_err());
    CHECK(exchange
              .emit_event(8, PhysicsEvent{PhysicsEvent::StateChanged{PhysicalStateId::PhysicsGround,
                                                                     PhysicalStateId::PhysicsAir}})
              .is_ok());
    CHECK(exchange.request_state(PhysicalStateId::PhysicsAir).is_ok());
    CHECK_EQ(exchange.commands.tick(), 8u);
    CHECK_EQ(exchange.commands.commands().size(), 1u);
    CHECK_EQ(exchange.events().size(), 1u);
}
