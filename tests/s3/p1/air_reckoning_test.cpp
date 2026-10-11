// Port of skate-core/src/air/reckoning/tests.rs.
#include <array>
#include <vector>

#include "harness.hpp"
#include "s3/core/air/body_flip.hpp"
#include "s3/core/air/body_spin.hpp"
#include "s3/core/air/reckoning.hpp"
#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/core/point_graph.hpp"
#include "s3/core/riding/ground_orientation.hpp"

namespace {

using namespace s3::air::reckoning;
using s3::air::body_flip::BodyFlipSettings;
using s3::air::body_spin::BodySpinSettings;
using s3::physics::skeleton_animation_record::IDENTITY;
using s3::point_graph::PointGraph;
using s3::riding::ground_orientation::GroundOrientationSettings;
namespace rs = s3::rs;

PointGraph<8> curve(float value) {
    PointGraph<8> g;
    g.x = {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f};
    g.y.fill(value);
    return g;
}

GroundOrientation orientation() {
    GroundOrientationSettings s;
    s.ground_normal_smoothing = {1.0f, 0.0f, 0.0f, 1.0f};
    s.up_vector_smoothing_slow = {0.2f, 0.1f, 0.05f, 0.3f};
    s.up_vector_smoothing_fast = {0.7f, 0.2f, 0.1f, 0.8f};
    s.dynamic_up_vs_ground_y = curve(0.0f);
    s.ground_vector_blend = curve(0.0f);
    s.deck_angle_usage_vs_speed = curve(0.0f);
    s.up_vector_smoothing_vs_speed = curve(0.0f);
    s.up_vector_max_delta_vs_speed = curve(1.0f);
    s.ground_blend_max_delta = 0.0f;
    s.up_vector_max_acceleration = 1.0f;
    s.anti_wobble_damping = 0.5f;
    s.extra_side_damping = 0.0f;
    s.minimum_wheels_for_ground_blend = 1;
    return GroundOrientation::new_(s);
}

Settings settings() {
    Settings s;
    s.ground_normal_smoothing = {1.0f, 0.0f, 0.0f, 1.0f};
    s.max_up_angle_delta = curve(0.5f);
    s.tilt_vs_rotation = curve(0.0f);
    s.tilt_vs_slope = curve(0.0f);
    s.body_spin.derivative_floor = 0.4f;
    s.body_spin.acceleration_limit = 0.2f;
    s.body_spin.curves.fill(curve(1.0f));
    s.body_spin.input_fade_threshold.fill(rs::from_bits(0x37800000u));
    s.body_flip.smoothing = 0.1f;
    s.body_flip.maximum_speed = 5.0f;
    s.body_flip.spin_scale = 0.5f;
    s.body_flip.missing_attribute_value = 0.0f;
    return s;
}

Input input() {
    Input i;
    i.landing_normal = {0.0f, 1.0f, 0.0f, 0.0f};
    i.normal_blend = 0.1f;
    i.target_spin = 0.0f;
    i.flip_request = 0.0f;
    i.com_to_deck = {0.0f, 1.0f, 0.0f, 0.0f};
    i.timestep = 1.0f / 60.0f;
    i.physical_body_spin = 0.0f;
    i.grind_adjusted_body_spin = 0.0f;
    i.additive_spin = false;
    i.direct_spin = true;
    i.reverse_stance = false;
    i.easy_body_spins = true;
    i.perfect_body_flips = false;
    return i;
}

S3_TEST(angle_limit_rotates_from_toward_target_and_preserves_parallel_branch) {
    const std::array<float, 4> target{0.0f, 0.0f, 1.0f, 0.0f};
    const std::array<float, 4> from{1.0f, 0.0f, 0.0f, 0.0f};
    const auto result = math::limit_angle(target, from, rs::f32::consts::FRAC_PI_4);
    CHECK(rs::abs(result[0] - rs::f32::consts::FRAC_1_SQRT_2) < 0.0001f);
    CHECK(rs::abs(result[2] - rs::f32::consts::FRAC_1_SQRT_2) < 0.0001f);
    CHECK_EQ(math::limit_angle({-2.0f, 0.0f, 0.0f, 0.0f}, from, 0.1f),
             (std::array<float, 4>{-2.0f, 0.0f, 0.0f, 0.0f}));
}

S3_TEST(direct_and_additive_spin_use_fixed_angle_step_and_keep_body_spin_history) {
    GroundOrientation o = orientation();
    ReckoningFrames frames = ReckoningFrames::new_();
    BodySpinState spin = BodySpinState::new_();
    AirState state = AirState::new_();
    Input in = input();
    in.target_spin = 2.0f;
    in.timestep = 0.1f;
    const auto old_history = spin.words();
    update(o, frames, spin, state, settings(), in);
    CHECK_EQ(state.spin_angle, 2.0f * rs::from_bits(0x3c888889u));
    CHECK(spin.words() == old_history);
    // Heading uses caller dt even though accumulated spin angle uses1/60.
    CHECK(rs::abs(frames.heading[0] - rs::cos(0.2f)) < 0.0001f);
    CHECK(rs::abs(frames.heading[2] + rs::sin(0.2f)) < 0.0001f);
    in.additive_spin = true;
    in.grind_adjusted_body_spin = 3.0f;
    update(o, frames, spin, state, settings(), in);
    CHECK_EQ(state.spin_speed, 5.0f);
    CHECK(spin.words() == old_history);
}

S3_TEST(airborne_filter_update_retains_controls_and_publishes_current_normal) {
    GroundOrientation o = orientation();
    ReckoningFrames frames = ReckoningFrames::new_();
    BodySpinState spin = BodySpinState::new_();
    AirState state = AirState::new_();
    Input in = input();
    const std::vector<std::uint32_t> slow_control(o.slow_filter.words().begin(), o.slow_filter.words().begin() + 4);
    const std::vector<std::uint32_t> fast_control(o.fast_filter.words().begin(), o.fast_filter.words().begin() + 4);
    in.landing_normal = {0.0f, 0.8f, 0.6f, 0.0f};
    update(o, frames, spin, state, settings(), in);
    CHECK(o.ground_normal.z > 0.59f);
    CHECK(o.up.z > 0.0f && o.up.z < o.ground_normal.z);
    CHECK((std::vector<std::uint32_t>(o.slow_filter.words().begin(), o.slow_filter.words().begin() + 4)) ==
          slow_control);
    CHECK((std::vector<std::uint32_t>(o.fast_filter.words().begin(), o.fast_filter.words().begin() + 4)) ==
          fast_control);
    const auto up_bits = rs::to_bits(lanes(o.up));
    CHECK((std::vector<std::uint32_t>(o.slow_filter.words().begin() + 4, o.slow_filter.words().begin() + 8)) ==
          (std::vector<std::uint32_t>(up_bits.begin(), up_bits.end())));
    CHECK_EQ(frames.ground[1], lanes(o.ground_normal));
}

S3_TEST(active_zero_flip_preserves_combined_frame_and_spin_reset_preserves_flip) {
    GroundOrientation o = orientation();
    ReckoningFrames frames = ReckoningFrames::new_();
    BodySpinState spin = BodySpinState::new_();
    AirState state = AirState::new_();
    Input in = input();
    state.flip_active = true;
    in.target_spin = 1.0f;
    update(o, frames, spin, state, settings(), in);
    CHECK(frames.body_flip == IDENTITY);
    CHECK_EQ(frames.heading, (std::array<float, 4>{1.0f, 0.0f, 0.0f, 0.0f}));
    state.flip_angle = 0.3f;
    state.flip_speed = 0.2f;
    state.reset_spin();
    CHECK_EQ((std::array<float, 2>{state.spin_angle, state.spin_speed}), (std::array<float, 2>{0.0f, 0.0f}));
    CHECK_EQ((std::array<float, 2>{state.flip_angle, state.flip_speed}), (std::array<float, 2>{0.3f, 0.2f}));
    CHECK(state.flip_active);
}

}  // namespace
