// Port of skate-core/src/physics/tests/board_dynamic_normal_history.rs.
// Regressions for the paired source boundary, not original-game fidelity proof.
#include "harness.hpp"
#include "s3/core/physics/board_dynamic_normal.hpp"

namespace {
using namespace s3::physics::board_dynamic_normal;
using s3::physics::board::BODY_COUNT;
constexpr Vector3 ZERO = Vector3{0.0f, 0.0f, 0.0f};

DynamicNormalSettings settings() {
    DynamicNormalSettings s;
    s.speed_damping = 0.0f;
    s.up_vector_damping = 1.0f;
    s.maximum_delta = 10.0f;
    s.speed_scale = 1.0f;
    s.maximum_delta_vs_speed = PointGraph<8>{{0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f},
                                             {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f}};
    return s;
}
std::array<Vector3, BODY_COUNT> filled(Vector3 v) { return {v, v, v, v, v, v, v}; }

S3_TEST(retained_normal_consumes_sampled_acceleration) {
    BoardGroundState contacts = BoardGroundState::default_();
    contacts.wheel_contact_count = 1;
    contacts.parts[0].in_contact = true;
    contacts.sample_accelerations(filled(Vector3{1.0f, 0.0f, 0.0f}), 1.0f);
    BoardDynamicNormal filter = BoardDynamicNormal::new_();
    filter.update(contacts, ZERO, 0.0f, settings());
    CHECK(filter.normal.x > 0.9999f);
    CHECK(s3::rs::abs(filter.normal.y) < 0.0001f && s3::rs::abs(filter.normal.z) < 0.0001f);
    const Vector3 once = filter.normal;
    // There is no live-body/timestep input or second history to re-sample.
    filter.update(contacts, ZERO, 0.0f, settings());
    CHECK(s3::rs::abs(filter.normal.x - once.x) < 0.0001f);
}

S3_TEST(body_history_reset_does_not_reset_retained_normal_or_delta) {
    BoardGroundState contacts = BoardGroundState::default_();
    contacts.sample_accelerations(filled(Vector3{5.0f, 0.0f, 0.0f}), 1.0f);
    BoardDynamicNormal filter = BoardDynamicNormal::new_();
    filter.normal = Vector3{0.6f, 0.8f, 0.0f};
    filter.delta = Vector3{0.1f, 0.2f, 0.3f};
    filter.update(contacts, ZERO, 0.0f, settings());
    const Vector3 retained_normal = filter.normal;
    const Vector3 retained_delta = filter.delta;
    contacts = BoardGroundState::default_();
    filter.update(contacts, ZERO, 0.0f, settings());
    CHECK_EQ(filter.normal, retained_normal);
    CHECK_EQ(filter.delta, retained_delta);
    contacts.wheel_contact_count = 1;
    contacts.parts[0].in_contact = true;
    contacts.sample_accelerations(filled(Vector3{2.0f, 0.0f, 0.0f}), 1.0f);
    filter.update(contacts, ZERO, 0.0f, settings());
    // Reusing the old velocity5 would produce acceleration-3 instead of+2.
    CHECK(filter.normal.x > 0.9999f);
    CHECK(s3::rs::abs(filter.normal.y) < 0.0001f && s3::rs::abs(filter.normal.z) < 0.0001f);
}

S3_TEST(fourth_wheel_contact_does_not_enter_native_three_wheel_sum) {
    BoardGroundState contacts = BoardGroundState::default_();
    contacts.wheel_contact_count = 1;
    contacts.parts[3].in_contact = true;
    contacts.accelerations[3] = Vector3{9.0f, 0.0f, 0.0f};
    BoardDynamicNormal filter = BoardDynamicNormal::new_();
    filter.update(contacts, ZERO, 0.0f, settings());
    CHECK(s3::rs::abs(filter.normal.x) < 0.0001f);
}
}  // namespace
