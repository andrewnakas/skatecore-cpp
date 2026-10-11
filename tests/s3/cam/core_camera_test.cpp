// skate-core camera/** has no Rust #[test]s. These are stage-K sanity checks of the port's
// native-arithmetic helpers (no Rust counterpart): ranges, identities and invariants only.
#include <cmath>

#include "harness.hpp"
#include "s3/core/camera.hpp"
#include "s3/core/camera/orientation_math.hpp"
#include "s3/core/camera/shot_orientation.hpp"
#include "s3/game/camera/stock_names.hpp"

using namespace s3::camera;
namespace rs = s3::rs;

S3_TEST(camera_normalize_angle_wraps_into_minus_pi_pi) {
    const float pi = rs::from_bits(0x40490fdb);
    CHECK_EQ(normalize_angle(0.5f), 0.5f);
    CHECK_EQ(normalize_angle(-pi), -pi);
    for (float a : {pi, 4.0f, -4.0f, 10.0f, -100.0f, 1000.0f}) {
        const float r = normalize_angle(a);
        CHECK_MSG(r >= -pi && r < pi, a << " -> " << r);
        const float turns = (a - r) / rs::from_bits(0x40c90fdb);
        CHECK_MSG(std::fabs(turns - std::round(turns)) < 1e-3f, a << " -> " << r);
    }
    CHECK_PANICS(normalize_angle(rs::f32::INFINITY_));
}

S3_TEST(camera_wrap_vmx_and_direction_to_angles) {
    const float pi = rs::from_bits(0x40490fdb);
    for (float a : {0.25f, 3.5f, -3.5f, 7.0f}) {
        const float r = rig_tracking::wrap_vmx(a);
        CHECK(r >= -pi - 1e-6f && r <= pi + 1e-6f);
    }
    // +Z has zero heading and zero elevation; +X a quarter turn.
    const F4 z = direction_to_angles({0.0f, 0.0f, 1.0f, 0.0f});
    CHECK(std::fabs(z[0]) < 1e-5f && std::fabs(z[1]) < 1e-5f);
    const F4 x = direction_to_angles({1.0f, 0.0f, 0.0f, 0.0f});
    CHECK(std::fabs(x[1] - rs::from_bits(0x3fc90fdb)) < 1e-4f);
    CHECK_EQ(direction_to_angles({0.0f, 1.0f, 0.0f, 0.0f}), (F4{0.0f, 0.0f, 0.0f, 0.0f}));
}

S3_TEST(camera_scalar_tracker_converges_without_overshoot_limits) {
    ScalarTracker t{};
    ScalarTrackerParameters p;
    p.speed_clamp = 100.0f;
    p.acceleration_clamp_min = 1000.0f;
    p.acceleration_clamp_max = 1000.0f;
    p.smoothing_min = 0.5f;
    p.smoothing_max = 0.5f;
    p.overshoot_zeroes_velocity = true;
    for (int i = 0; i < 600; ++i) t.update(1.0f / 60.0f, 2.0f, p);
    CHECK_MSG(std::fabs(t.position - 2.0f) < 1e-3f, t.position);
}

S3_TEST(camera_shot_blend_helpers) {
    CHECK_EQ(interpolate_float(1.0f, 3.0f, 1, 0.25f), 3.0f);
    CHECK_EQ(interpolate_float(1.0f, 3.0f, 7, 0.25f), 1.0f);
    CHECK(std::fabs(interpolate_float(1.0f, 3.0f, 0, 1.0f) - 3.0f) < 1e-6f);
    CHECK(!blend_interval({0.0f, 1.0f, 2.0f}, {false, true, true}, 0.5f).has_value());
    auto first = blend_interval({0.0f, 1.0f, 2.0f}, {true, true, true}, 0.5f);
    CHECK(first.has_value() && std::get<0>(*first) == 0 && std::get<1>(*first) == 1 &&
          std::get<2>(*first) == 0.5f);
    auto below = blend_interval({1.0f, 2.0f, 3.0f}, {true, true, false}, 0.0f);
    CHECK(below.has_value() && std::get<0>(*below) == 0 && std::get<1>(*below) == 0);
    auto above = blend_interval({1.0f, 2.0f, 3.0f}, {true, true, false}, 9.0f);
    CHECK(above.has_value() && std::get<0>(*above) == 0 && std::get<1>(*above) == 1 && std::get<2>(*above) == 1.0f);
    CHECK_EQ(filter_blend_value(0.0f, 10.0f, 0.0f, -1.0f, 5.0f), 5.0f);
    CHECK_EQ(Shot::new_(), Shot{});
}

S3_TEST(camera_slerp_endpoints_and_position_from_angles) {
    const F4 identity = {0.0f, 0.0f, 0.0f, 1.0f};
    const F4 q = orientation_math::quaternion_from_angles(0.0f, 1.0f, 0.0f);
    const F4 a = shot_orientation::slerp(identity, q, 0.0f);
    const F4 b = shot_orientation::slerp(identity, q, 1.0f);
    for (std::size_t i = 0; i < 4; ++i) {
        CHECK(std::fabs(a[i] - identity[i]) < 1e-4f);
        CHECK(std::fabs(b[i] - q[i]) < 1e-4f);
    }
    // Zero elevation/heading puts the camera `distance` along +Z from the anchor.
    const F4 p = position_from_angles({1.0f, 2.0f, 3.0f, 0.0f}, F4{}, 0.0f, 0.0f, 4.0f, 0, 0.0f);
    CHECK(std::fabs(p[0] - 1.0f) < 1e-5f && std::fabs(p[1] - 2.0f) < 1e-5f && std::fabs(p[2] - 7.0f) < 1e-5f);
    const F4 clamped = position_from_angles({0.0f, -5.0f, 0.0f, 0.0f}, F4{}, 0.0f, 0.0f, 1.0f, 1, 0.5f);
    CHECK_EQ(clamped[1], 0.5f);
}

S3_TEST(camera_anchors_reset_and_velocity) {
    Anchors anchors = Anchors::new_();
    AnchorInputs input;
    input.board_position = {1.0f, 0.0f, 0.0f, 0.0f};
    input.board_offset_direction = {0.0f, 1.0f, 0.0f, 0.0f};
    input.reset = true;
    anchors.update(input);
    CHECK_EQ(anchors.entries[0].velocity, (F4{0.0f, 0.0f, 0.0f, 0.0f}));
    CHECK_EQ(anchors.entries[3].position[1], 0.5f);
    input.reset = false;
    input.board_position = {1.1f, 0.0f, 0.0f, 0.0f};
    anchors.update(input);
    CHECK(std::fabs(anchors.entries[0].velocity[0] - 6.0f) < 1e-3f);
}

S3_TEST(camera_stock_names_lookup8_basics) {
    using s3::game::camera::stock_names::lookup8;
    CHECK_EQ(lookup8(std::string()), std::uint64_t(0));
    CHECK_NE(lookup8(std::string("a")), lookup8(std::string("b")));
    // 24-byte blocks plus remainder take different paths; both must be deterministic.
    const std::string long_name = "camera_shots_long_name_over_24_bytes";
    CHECK_EQ(lookup8(long_name), lookup8(long_name));
}
