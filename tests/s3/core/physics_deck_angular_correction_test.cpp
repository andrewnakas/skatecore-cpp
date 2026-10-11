// Port of the inline tests in skate-core/src/physics/deck_angular_correction.rs.
#include "harness.hpp"
#include "s3/core/physics/deck_angular_correction.hpp"

namespace {
using namespace s3::physics::deck_angular_correction;
using s3::physics::rigid_body::RetailQuaternion;
using s3::rs::abs;

RetailBodyRates body() {
    const Basis3 identity{{{{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}}};
    return RetailBodyRates{
        RetailQuaternion::IDENTITY,
        identity,
        Basis3{{{{3.0f, 0.2f, 0.1f}, {0.2f, 2.0f, 0.3f}, {0.1f, 0.3f, 1.0f}}}},
        Vector3::ZERO,
        Vector3::ZERO,
        Vector3::ZERO,
        Vector3::ZERO,
        Vector3::ZERO,
        0.0f,
        3,
    };
}

S3_TEST(thrown_board_correction_brakes_axis_overshoot_without_touching_other_motion) {
    RetailBodyRates state = body();
    state.angular_velocity = Vector3{4.0f, 3.0f, 5.0f};
    apply_axis_displacement(state, Vector3{0.0f, 0.02f, 0.0f});
    CHECK(abs(state.torque_acceleration.y + 108.0f) < 0.0001f);
    CHECK(abs(state.torque_acceleration.x) < 0.0001f);
    CHECK(abs(state.torque_acceleration.z) < 0.0001f);
    CHECK_EQ(state.angular_velocity, (Vector3{4.0f, 3.0f, 5.0f}));
    CHECK_EQ(state.cool_down, 0u);
    RetailBodyRates reverse = body();
    reverse.angular_velocity = Vector3{0.0f, -3.0f, 0.0f};
    apply_axis_displacement(reverse, Vector3{0.0f, 0.02f, 0.0f});
    CHECK(abs(reverse.torque_acceleration.y - 252.0f) < 0.0001f);
}

S3_TEST(correction_enters_accumulator_and_respects_existing_angular_motion) {
    const Vector3 request{0.02f, 0.01f, -0.03f};
    RetailBodyRates state = body();
    apply_angular_displacement(state, request);
    const std::pair<float, float> pairs[] = {
        {state.torque_acceleration.x, 72.0f},
        {state.torque_acceleration.y, 36.0f},
        {state.torque_acceleration.z, -108.0f},
    };
    for (const auto& [actual, want] : pairs) {
        CHECK(abs(actual - want) < 0.0001f);
    }
    CHECK_EQ(state.orientation, RetailQuaternion::IDENTITY);
    CHECK_EQ(state.cool_down, 0u);
    RetailBodyRates overshoot = body();
    overshoot.angular_velocity = Vector3{0.0f, 3.0f, 0.0f};
    apply_limited_displacement(overshoot, Vector3{0.0f, 0.02f, 0.0f});
    CHECK_EQ(overshoot.torque_acceleration, Vector3::ZERO);
    RetailBodyRates against = body();
    against.angular_velocity = Vector3{0.0f, -3.0f, 0.0f};
    apply_limited_displacement(against, Vector3{0.0f, 0.02f, 0.0f});
    CHECK(abs(against.torque_acceleration.y - 72.0f) < 0.0001f);
    RetailBodyRates ground = body();
    ground.basis = Basis3{{{{0.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}}};
    apply_ground_body_torque(ground);
    CHECK(abs(ground.torque_acceleration.x + 1.815f) < 0.00001f);
    CHECK(abs(ground.torque_acceleration.y + 0.121f) < 0.00001f);
    CHECK(abs(ground.torque_acceleration.z + 0.0605f) < 0.00001f);
    CHECK_EQ(ground.force_acceleration, Vector3::ZERO);
}
}  // namespace
