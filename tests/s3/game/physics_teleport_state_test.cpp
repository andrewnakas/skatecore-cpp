// Port of skate-game/src/physics/teleport_state.rs `mod vehicle_tests`.
#include "harness.hpp"
#include "s3/game/physics/teleport_state.hpp"

using namespace s3::game::physics::teleport_state;
using V3 = std::array<float, 3>;

static AnimationPartTransform matrix() {
    return {{
        {1.f, 0.f, 0.f, 0.f},
        {0.f, 1.f, 0.f, 0.f},
        {0.f, 0.f, 1.f, 0.f},
        {3.f, 4.f, 5.f, 0.f},
    }};
}

S3_TEST(vehicle_momentum_survives_reset_reply_and_is_consumed_once) {
    auto state = Runtime::new_(Checkpoint{matrix(), true});
    state.request_vehicle_ejection(matrix(), {12.f, 2.f, -3.f}, {0.f, 1.f, 0.f});
    CHECK(state.take_reply().has_value());
    CHECK(state.take_manual_on_board() == std::optional<bool>(false));
    const auto ejection = state.take_vehicle_ejection();
    CHECK(ejection.has_value());
    CHECK(ejection->first == (V3{12.f, 2.f, -3.f}));
    CHECK(ejection->second == (V3{0.f, 1.f, 0.f}));
    CHECK(!state.take_vehicle_ejection().has_value());
    state.request_vehicle_ejection(matrix(), {12.f, 2.f, -3.f}, {0.f, 1.f, 0.f});
    state.request_manual(matrix(), true);
    CHECK(!state.take_vehicle_ejection().has_value());
}
