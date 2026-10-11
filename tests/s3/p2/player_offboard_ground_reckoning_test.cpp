// Port of skate-core/src/player/offboard/ground_reckoning/tests.rs.
#include <cmath>
#include <tuple>

#include "harness.hpp"
#include "s3/core/player/offboard/ground_reckoning.hpp"

namespace {
using namespace s3::player::offboard::ground_reckoning;
using s3::air::body_spin::BodySpinState;
using s3::air::ground_normal::GroundNormalFilter;
using s3::air::reckoning::data::AirState;
using s3::math::Vector3;
using s3::point_graph::PointGraph;
using s3::riding::ground_orientation::GroundOrientation;
using s3::riding::reckoning_frames::ReckoningFrames;

GroundOrientation orientation() {
    const Vector up{0.0f, 1.0f, 0.0f, 0.0f};
    GroundOrientation o;
    o.dynamic_up = xyz(up);
    o.up = xyz(up);
    o.target = xyz(up);
    o.up_velocity = Vector3::new_(7.0f, 8.0f, 9.0f);
    o.ground_normal = xyz(up);
    o.ground_blend = 0.75f;
    o.ground_filter = GroundNormalFilter::initialized({1.0f, 0.0f, 0.0f, 1.0f}, up);
    o.slow_filter = GroundNormalFilter::initialized({0.2f, 0.1f, 0.05f, 0.3f}, up);
    o.fast_filter = GroundNormalFilter::initialized({0.7f, 0.2f, 0.1f, 0.8f}, up);
    return o;
}

std::tuple<Output, BodySpinState, AirState> run(GroundOrientation& o, const Input& input) {
    PointGraph<8> curve;
    curve.x = {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f};
    auto frames = ReckoningFrames::new_();
    auto spin = BodySpinState::new_();
    auto air = AirState::new_();
    air.spin_angle = 2.0f;
    air.secondary_lean_angle = 3.0f;
    const auto output = update(o, frames, spin, air, Settings{{1.0f, 0.0f, 0.0f, 1.0f}, &curve, &curve}, input);
    return {output, spin, air};
}

Input input() {
    return Input{{0.0f, 1.0f, 0.0f, 2.0f}, {0.0f, 0.0f, 1.0f, 4.0f}, {1.0f, 0.0f, 0.0f, 0.0f}, 0.5f, false, true, 0.25f};
}

S3_TEST(blend_preserves_fourth_lane_and_updates_history_without_resetting_other_state) {
    auto state = orientation();
    std::array<std::uint32_t, 4> controls{};
    for (int k = 0; k < 4; ++k) controls[k] = state.slow_filter.words()[k];
    auto [out, spin, air] = run(state, input());
    CHECK(std::fabs(out.up_1152[1] - s3::rs::f32::consts::FRAC_1_SQRT_2) < 0.00001f);
    CHECK(std::fabs(out.up_1152[3] - 3.0f * 1.41421356237309504880168872420969808f) < 0.00001f);
    CHECK_EQ(out.dynamic_up_1136, (Vector{0.0f, 1.0f, 0.0f, 2.0f}));
    CHECK_EQ(out.velocity_1184, Vector{});
    CHECK_EQ(state.up_velocity, Vector3::ZERO);
    CHECK_EQ(state.ground_blend, 0.75f);
    for (int k = 0; k < 4; ++k) {
        CHECK_EQ(state.slow_filter.words()[k], controls[k]);
        CHECK_EQ(state.slow_filter.words()[4 + k], s3::rs::to_bits(out.up_1152[k]));
        CHECK_EQ(state.fast_filter.words()[8 + k], s3::rs::to_bits(out.up_1152[k]));
    }
    bool any = false;
    for (int k = 12; k < 16; ++k) any = any || state.slow_filter.words()[k] != 0;
    CHECK(any);
    CHECK_EQ(s3::rs::from_bits(spin.words()[30]), 0.25f);
    CHECK_EQ(air.secondary_lean_angle, 0.0f);
    CHECK_EQ(air.spin_angle, 2.0f);
}

S3_TEST(cancellation_retains_previous_up_and_disabled_spin_still_advances_history) {
    auto state = orientation();
    auto in = input();
    in.requested_up = {0.0f, -1.0f, 0.0f, -2.0f};
    in.enable_body_spin_input = false;
    const auto previous = in.previous_up;
    auto [out, spin, air] = run(state, in);
    (void)air;
    CHECK_EQ(out.up_1152, previous);
    CHECK_EQ(out.target_1168, previous);
    CHECK_EQ(s3::rs::from_bits(spin.words()[30]), 0.0f);
    CHECK_EQ(spin.words()[42], 1u);
}
}  // namespace
