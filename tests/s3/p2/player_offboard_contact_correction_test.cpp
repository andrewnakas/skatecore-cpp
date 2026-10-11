// Port of skate-core/src/player/offboard/contact_correction/tests.rs.
#include "harness.hpp"
#include "s3/core/player/offboard/contact_correction.hpp"

namespace {
using namespace s3::player::offboard::contact_correction;

ContactCorrection state() {
    // Explicit synthetic prior state; constructor/reset ownership is separate.
    return ContactCorrection{true, {0.0f, 0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}};
}
Input input(Vector a, Vector b) { return Input{{a, b}, Vector{}, {0.0f, 1.0f, 0.0f, 0.0f}}; }
void close(float actual, float expected) { CHECK_MSG(std::fabs(actual - expected) < 1e-6f, actual << " != " << expected); }

S3_TEST(inactive_contact_clears_displacement_but_retains_previous_direction) {
    auto s = state();
    s.update(input(Vector{}, {0.001f, 0.0f, 0.0f, 0.0f}));
    CHECK(!s.active);
    CHECK_EQ(s.direction, (Vector{0.0f, 0.0f, 1.0f, 0.0f}));
    CHECK_EQ(s.displacement, Vector{});
}

S3_TEST(equal_contacts_choose_second_and_apply_clearance_before_half_correction) {
    auto s = state();
    s.update(input({0.15f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -0.15f, 0.0f}));
    CHECK(s.active);
    close(s.direction[2], -1.0f);
    close(s.displacement[2], -0.05f);
    close(s.displacement[0], 0.0f);
}

S3_TEST(large_correction_is_limited_then_projected_off_current_up_axis) {
    auto s = state();
    s.update(input({3.0f, 4.0f, 0.0f, 0.0f}, Vector{}));
    close(s.direction[0], 0.6f);
    close(s.direction[1], 0.8f);
    close(s.displacement[0], 0.06f);
    close(s.displacement[1], 0.0f);
}

S3_TEST(projection_axis_removes_response_without_disabling_contact_observation) {
    auto s = state();
    auto i = input({0.0f, 0.0f, 0.2f, 0.0f}, Vector{});
    i.projection_axis_416 = {0.0f, 0.0f, 1.0f, 0.0f};
    s.update(i);
    CHECK(s.active);
    close(s.direction[2], 1.0f);
    CHECK_EQ(s.displacement, Vector{});
}
}  // namespace
