// Port of skate-game/src/physics/grind_camera/tests.rs (mounted as physics::grind_camera::tests).
#include <cmath>

#include "harness.hpp"
#include "s3/game/physics/grind_camera.hpp"

using namespace s3::game::physics::grind_camera;

namespace {
CameraGrindOutput output() {
    CameraGrindOutput o;
    o.direction_0 = {3.0f, 3.0f, 3.0f, 3.0f};
    o.camera_target_96 = {7.0f, 7.0f, 7.0f, 7.0f};
    o.grinding_316 = 1;
    return o;
}
Contact contact(float x, std::uint32_t family) {
    Contact c;
    c.point = {x, 0.0f, 0.0f, 0.0f};
    c.direction = {1.0f, 0.0f, 0.0f, 0.0f};
    c.endpoints = {V{0.0f, 0.0f, 0.0f, 0.0f}, V{20.0f, 0.0f, 0.0f, 0.0f}};
    c.family = family;
    return c;
}
}  // namespace

S3_TEST(ordinary_motion_is_not_smoothed_but_family_discontinuity_is) {
    GrindCamera camera{};
    auto out = output();
    camera.update(contact(1.0f, 0), out);
    camera.update(contact(2.0f, 0), out);
    CHECK_EQ(out.camera_target_96[0], 2.0f);
    camera.update(contact(10.0f, 1), out);
    CHECK(std::fabs(out.camera_target_96[0] - 3.35f) < 0.00001f);
    CHECK_EQ(out.grinding_316, 1);
}

S3_TEST(primitive_change_uses_xyz_midpoint_and_inactive_does_not_erase_output) {
    GrindCamera camera{};
    auto out = output();
    camera.update(contact(2.0f, 0), out);
    auto next = contact(10.0f, 0);
    next.endpoints[1][0] = 22.0f;
    camera.update(next, out);
    CHECK(std::fabs(out.camera_target_96[0] - 2.4f) < 0.00001f);
    const auto retained = out.camera_target_96;
    camera.update(std::nullopt, out);
    CHECK(out.camera_target_96 == retained);
    camera.update(contact(25.0f, 0), out);
    CHECK_EQ(out.camera_target_96[0], 25.0f);
}
