// Port of skate-core/src/player/offboard/controller/tests.rs.
#include "harness.hpp"
#include "s3/core/player/offboard/controller.hpp"

namespace {
using namespace s3::player::offboard::controller;
namespace movement_intent = s3::player::offboard::movement_intent;
namespace movement_velocity = s3::player::offboard::movement_velocity;

using Metrics = std::array<std::optional<ClipMetric>, 3>;
Metrics metrics() { return {ClipMetric{-2.0f, 1.0f}, ClipMetric{-4.0f, 1.0f}, ClipMetric{-8.0f, 1.0f}}; }
template <std::size_t N>
PointGraph<N> graph(float value) {
    PointGraph<N> g;
    for (std::size_t i = 0; i < N; ++i) g.x[i] = static_cast<float>(i);
    g.y.fill(value);
    return g;
}
Settings settings() {
    Settings s;
    s.movement_intent.sprint_speed = graph<4>(2.0f);
    s.movement_intent.normal_speed = graph<16>(1.0f);
    s.movement_intent.sprint_blend = graph<8>(0.0f);
    s.movement_intent.sprint_time_cap = 1.0f;
    s.movement_intent.slide_steering = graph<8>(0.0f);
    s.movement_velocity.slope_speed_scalar = graph<8>(1.0f);
    s.movement_velocity.slope_mode_speed = graph<8>(1.0f);
    s.movement_velocity.turn_vs_speed = graph<8>(0.0f);
    s.movement_velocity.turn_delta_vs_speed = graph<8>(1.0f);
    s.slide_vs_slope = graph<8>(0.0f);
    s.slide_vs_speed = graph<8>(0.0f);
    return s;
}
GroundJob job() {
    GroundJob j;
    j.contact_position = ZERO;
    j.contact_normal = UP;
    j.support_frame = IDENTITY;
    j.target_position = ZERO;
    j.target_normal = UP;
    j.edge_position = ZERO;
    j.edge_normal = UP;
    j.flags = 1;
    j.support_id = 0;
    j.collision_displacements = {ZERO, ZERO};
    j.animation_motion = {0.0f, 0.0f, 1.0f, 0.0f};
    j.animation_velocity = ZERO;
    j.desired_direction = {0.0f, 0.0f, 1.0f, 0.0f};
    j.animation_position = ZERO;
    j.requested_duration = 1.0f;
    j.mirrored = false;
    j.requested_phase = -1.0f;
    j.override_duration = 1.0f;
    j.animation_directed = false;
    j.movement = 1.0f;
    j.steering = 0.0f;
    j.target_frame = IDENTITY;
    return j;
}
bool finite(const Vector& v) {
    for (float x : v)
        if (!std::isfinite(x)) return false;
    return true;
}
bool finite(const Frame& f) {
    for (const auto& v : f)
        if (!finite(v)) return false;
    return true;
}

S3_TEST(constructor_uses_clip_metrics_and_original_initial_fields) {
    const auto s = State::new_(metrics());
    CHECK_EQ(s.thresholds._0, (std::array<float, 4>{0.01f, 3.0f, 6.0f, s3::rs::from_bits(0x501502f9u)}));
    CHECK_EQ(s.motion.frame_0, IDENTITY);
    CHECK_EQ(s.surface.surface_normal, UP);
    CHECK_EQ(s.velocity_override_remaining_772, -1.0f);
    CHECK_EQ(s.motion.target_scale_784, -1.0f);
    CHECK(!s.cadence.phase.duration.has_value());
}

S3_TEST(tiny_velocity_delta_keeps_ground_publication_finite) {
    auto controller = Controller::new_(settings(), metrics());
    controller.state.motion.velocity_480 = {0.0f, 1.0e-21f, 1.0f, 0.0f};
    controller.state.motion.speed_704 = 1.0f;
    const auto result = controller.step_ground(job());
    CHECK(finite(result.position));
    CHECK(finite(result.velocity));
    CHECK(finite(result.physical_frame));
    CHECK(finite(result.animation_frame));
    CHECK(finite(controller.state.surface.lean));
    CHECK(std::isfinite(controller.state.cadence.phase.phase));
}

S3_TEST(absent_clip_queries_have_original_zero_speed) {
    const auto s = State::new_({std::nullopt, std::nullopt, std::nullopt});
    CHECK_EQ(s.thresholds._0[1], 0.0f);
    CHECK_EQ(s.thresholds._0[2], 0.0f);
}

S3_TEST(reset_preserves_phase_and_correction_vectors_only) {
    auto s = State::new_(metrics());
    s.cadence.phase.phase = 0.75f;
    s.cadence.phase.duration = 2.0f;
    s.motion.correction_576 = {1.0f, 1.0f, 1.0f, 1.0f};
    s.correction_target_592 = {2.0f, 2.0f, 2.0f, 2.0f};
    s.motion.correction_enabled_711 = true;
    s.motion.velocity_480 = {3.0f, 3.0f, 3.0f, 3.0f};
    s.intent.speed = 9.0f;
    s.reset();
    CHECK_EQ(s.cadence.phase.phase, 0.75f);
    CHECK(s.cadence.phase.duration == std::optional<float>(2.0f));
    CHECK_EQ(s.motion.correction_576, (Vector{1.0f, 1.0f, 1.0f, 1.0f}));
    CHECK_EQ(s.correction_target_592, (Vector{2.0f, 2.0f, 2.0f, 2.0f}));
    CHECK(!s.motion.correction_enabled_711);
    CHECK_EQ(s.motion.velocity_480, ZERO);
    CHECK_EQ(s.intent.speed, 0.0f);
}

PlacementInput placement(std::uint32_t current, std::uint32_t previous) {
    PlacementInput p;
    p.frame = IDENTITY;
    p.velocity = {3.0f, 0.0f, 4.0f, 0.0f};
    p.body_position = {1.0f, 2.0f, 3.0f, 0.0f};
    p.current_state = current;
    p.previous_state = previous;
    p.previous_frame = IDENTITY;
    return p;
}

S3_TEST(landing_preserves_eligible_correction_from_air_target) {
    auto s = State::new_(metrics());
    s.correction_target_592 = {0.0f, 0.0f, 2.0f, 0.0f};
    s.place(placement(500, 501));
    CHECK(s.motion.correction_enabled_711);
    CHECK_EQ(s.motion.correction_576, (Vector{0.0f, 0.0f, -2.0f, 0.0f}));
    CHECK_EQ(s.position_368, (Vector{1.0f, 2.0f, 3.0f, 0.0f}));
    CHECK_EQ(s.motion.speed_704, 5.0f);
}

S3_TEST(air_placement_clears_correction_without_resetting_movement_timers) {
    auto s = State::new_(metrics());
    s.intent.sprint_time = 4.0f;
    s.correction_target_592 = {1.0f, 1.0f, 1.0f, 1.0f};
    s.motion.correction_enabled_711 = true;
    s.place(placement(501, 500));
    CHECK_EQ(s.intent.sprint_time, 4.0f);
    CHECK_EQ(s.correction_target_592, ZERO);
    CHECK(!s.motion.correction_enabled_711);
}

S3_TEST(other_placement_keeps_existing_correction) {
    auto s = State::new_(metrics());
    s.motion.correction_576 = {1.0f, 1.0f, 1.0f, 1.0f};
    s.motion.correction_enabled_711 = true;
    s.place(placement(600, 500));
    CHECK_EQ(s.motion.correction_576, (Vector{1.0f, 1.0f, 1.0f, 1.0f}));
    CHECK(s.motion.correction_enabled_711);
}

S3_TEST(full_normal_dispatch_moves_canonical_velocity_and_publishes) {
    auto c = Controller::new_(settings(), metrics());
    const auto result = c.step_ground(job());
    CHECK(!result.alternate);
    CHECK(c.state.motion.velocity_480[2] > 0.0f);
    CHECK(result.physical_frame[3][2] > 0.0f);
    CHECK_EQ(result.velocity, c.state.frame_output.velocity);
    CHECK_EQ(result.position, c.state.position_368);
}

S3_TEST(alternate_dispatch_skips_cadence_and_position_but_exports) {
    auto c = Controller::new_(settings(), metrics());
    auto j = job();
    j.flags = 2;
    j.target_position = {0.0f, 0.0f, 1.0f, 0.0f};
    c.state.motion.velocity_480 = {0.0f, 5.0f, 0.0f, 0.0f};
    c.state.frame_output.velocity = {0.0f, 5.0f, 0.0f, 0.0f};
    c.state.position_368 = {9.0f, 9.0f, 9.0f, 9.0f};
    c.state.cadence.phase.phase = 0.75f;
    const auto r = c.step_ground(j);
    CHECK(r.alternate);
    CHECK_EQ(c.state.position_368, (Vector{9.0f, 9.0f, 9.0f, 9.0f}));
    CHECK_EQ(c.state.cadence.phase.phase, 0.75f);
    CHECK(c.state.motion.velocity_480[1] < 5.0f);
}

S3_TEST(exporter_degenerate_frame_is_identity) {
    CHECK_EQ(build_frame(UP, UP), IDENTITY);
    const auto f = build_frame({0.0f, 2.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 3.0f, 0.0f});
    for (int axis = 0; axis < 3; ++axis)
        for (int lane = 0; lane < 3; ++lane) CHECK(s3::rs::abs(f[axis][lane] - IDENTITY[axis][lane]) < 1e-6f);
    CHECK_EQ(f[3], ZERO);
}
}  // namespace
