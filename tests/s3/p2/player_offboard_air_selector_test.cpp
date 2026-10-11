// Port of skate-core/src/player/offboard/air_selector/tests.rs.
#include <vector>

#include "harness.hpp"
#include "s3/core/player/offboard/air_selector.hpp"

namespace {
using namespace s3::player::offboard::air_selector;
using namespace s3::player::offboard::air_selector::math;
using s3::math::Vector3;
namespace ground_query = s3::player::offboard::ground_query;

Settings settings() { return Settings{0.9f, 0.3f, 3}; }
Packet packet() {
    auto p = Packet::initialized(0.0f);
    p.position_32 = {0.0f, 2.0f, 0.0f, 0.0f};
    p.velocity_0 = {0.0f, 3.0f, 4.0f, 0.0f};
    p.forward_64 = {0.0f, 0.0f, 1.0f, 0.0f};
    p.scalar_100 = 0.2f;
    return p;
}

S3_TEST(a_nan_in_the_unused_fourth_lane_does_not_reject_the_packet) {
    auto p = packet();
    p.position_32[3] = s3::rs::f32::NAN_;
    p.secondary_velocity_16[3] = s3::rs::f32::NAN_;
    const auto outcome = candidates::prepare(p, {0.0f, -9.8f, 0.0f, s3::rs::f32::NAN_}, settings());
    CHECK(outcome.is_ok());
}

S3_TEST(a_nan_in_a_meaningful_lane_still_rejects_the_packet) {
    for (int lane = 0; lane < 3; ++lane) {
        auto p = packet();
        p.position_32[lane] = s3::rs::f32::NAN_;
        const auto r = candidates::prepare(p, {0.0f, -9.8f, 0.0f, 0.0f}, settings());
        CHECK(r.is_err());
        CHECK_EQ(std::string(r.unwrap_err()), std::string("Nonfinite BipedAir launch packet"));
    }
}

Context context() { return Context{0, -1, UP, {0.0f, 0.0f, 1.0f, 0.0f}}; }
QueryResult hit(std::int32_t frame, Vector position) {
    auto r = QueryResult::miss();
    r.contact_frame = frame;
    r.contact_time = static_cast<float>(frame) * DT;
    r.contact_position = position;
    r.surface = 7u << 7;
    return r;
}
Selector launched() {
    auto s = Selector::default_();
    s.begin_launch(packet(), {0.0f, -9.8f, 0.0f, 0.0f}, settings()).unwrap();
    return s;
}
Selector selected() {
    auto s = launched();
    s.observe_launch({hit(60, {0.0f, 0.0f, 4.0f, 0.0f})}).unwrap();
    s.select_launch(context(), s.candidates[0], std::nullopt).unwrap();
    return s;
}
void close(Vector a, Vector b) {
    for (int i = 0; i < 4; ++i) CHECK(s3::rs::abs(a[i] - b[i]) < 0.0001f);
}

S3_TEST(constructor_reset_and_exit_have_distinct_native_effects) {
    {
        auto s = Selector::default_();
        CHECK(!s.sampling.restart_allowed_8493);
        CHECK_EQ(s.sampling.selection.landing_frame_8480, 0);
        s.launch = packet();
        s.reset();
        CHECK(s.launch == packet());
        CHECK(s.sampling.restart_allowed_8493);
        CHECK_EQ(s.sampling.selection.landing_frame_8480, 1000);
    }
    auto s = selected();
    const auto before = s.predictions;
    const auto candidate = s.selected_candidate.trajectory;
    s.exit();
    CHECK(!s.sampling.pending_8492 && !s.sampling.preinitialized_8494);
    CHECK(s.predictions == before);
    CHECK(s.selected_candidate.trajectory == candidate);
}

S3_TEST(launch_requests_are_native_valid_and_index_shifted) {
    const auto s = launched();
    const auto p = packet();
    const auto trajectory = s.candidates[0].trajectory;
    const Trajectory original{add(p.position_32, s.offset_8272), sub(p.velocity_0, {0.0f, 0.1f, 0.0f, 0.0f}),
                              {0.0f, -9.8f, 0.0f, 0.0f}, 2.0f};
    close(trajectory.position, original.position_at(3.0f * DT));
    close(trajectory.velocity, original.velocity_at(3.0f * DT));
    const auto q = s.predictions[0].request;
    CHECK_EQ(q.trajectory.duration, 2.0f);
    CHECK_EQ(q.radius, 0.3f);
    CHECK_EQ(q.start_error, 0.25f);
    CHECK_EQ(q.end_error, 1.2f);
    CHECK(validate_request(q).is_ok());
}

S3_TEST(invalid_authoring_is_not_capped_or_padded) {
    auto s = Selector::default_();
    auto p = packet();
    p.kind_108 = 17;
    CHECK(s.begin_launch(p, {0.0f, -9.8f, 0.0f, 0.0f}, settings()).is_err());
    p.kind_108 = 0;
    CHECK(s.begin_launch(p, {0.0f, -9.8f, 0.0f, 0.0f}, settings()).is_err());
    auto bad = settings();
    bad.sphere_radius = 0.0f;
    CHECK(s.begin_launch(packet(), {0.0f, -9.8f, 0.0f, 0.0f}, bad).is_err());
    CHECK(s.candidates.empty());
}

S3_TEST(pending_sample_uses_original_fallback_without_consuming) {
    auto s = launched();
    const auto fallback = s.sampling.fallback_8208;
    PointGraph<8> graph;
    graph.x.fill(0.0f);
    graph.y.fill(1.0f);
    TrajectoryResult out;
    s.sample(10, DT, graph, out);
    close(out.position_272, fallback.position_at(10.0f * DT));
    CHECK(!out.valid_404);
    CHECK_EQ(out.normal_304, UP);
    CHECK(s.sampling.pending_8492);
    CHECK_EQ(out.scalar_392, packet().scalar_100);
}

S3_TEST(scoring_keeps_first_tie_and_secondary_low_height_penalty) {
    auto p = packet();
    p.kind_108 = 2;
    p.kind_112 = 1;
    p.secondary_velocity_16 = {0.0f, 0.0f, 100.0f, 0.0f};
    auto s = Selector::default_();
    s.begin_launch(p, {0.0f, -9.8f, 0.0f, 0.0f}, settings()).unwrap();
    std::vector<QueryResult> results;
    for (const auto& c : s.candidates) results.push_back(hit(60, add(c.trajectory.position, {0.0f, 0.1f, 2.0f, 0.0f})));
    s.observe_launch(results).unwrap();
    s.select_launch(context(), s.candidates[0], std::nullopt).unwrap();
    CHECK(s.selected_index == std::optional<std::size_t>(0));
    CHECK(s.scores[2] < -9000.0f);
    CHECK(recovered::selection::selected_index(std::vector<float>{2.0f, 2.0f, 1.0f}) ==
          std::optional<std::size_t>(0));
}

S3_TEST(selection_preserves_special_surface_and_unshifted_candidate) {
    auto s = launched();
    s.observe_launch({hit(60, {0.0f, 0.0f, 4.0f, 0.0f})}).unwrap();
    const auto first = s.candidates[0];
    const ground_query::Edge edge{Vector3::new_(-1.0f, 0.0f, 4.0f), Vector3::new_(1.0f, 0.0f, 4.0f)};
    const ledge::Adjustment adjustment{edge, {0.0f, 0.0f, 4.0f, 0.0f}, first.trajectory, 55, 0.3f};
    s.select_launch(context(), first, adjustment).unwrap();
    CHECK_EQ(s.sampling.selection.word_8396, 2u);
    CHECK_EQ(s.predictions[0].result.contact_frame, 63);
    CHECK_EQ(s.sampling.selection.landing_frame_8480, 55);
    CHECK_EQ(s.selected_candidate.trajectory.velocity, first.trajectory.velocity);
    CHECK_NE(s.sampling.selection.trajectory_8144.position, s.selected_candidate.trajectory.position);
}

S3_TEST(animation_adjusts_sample_only_and_preserves_current_point) {
    auto s = selected();
    const auto arc = s.sampling.selection.trajectory_8144;
    const auto candidate = s.selected_candidate.trajectory;
    const auto prediction = s.predictions[0];
    const std::int32_t frame = 10;
    const auto before = arc.position_at(static_cast<float>(frame) * DT);
    const Frame axes{{{1.0f, 0.0f, 0.0f, 0.0f}, UP, {0.0f, 0.0f, 1.0f, 0.0f}, {}}};
    s.adjust_animation(frame, {0.0f, 0.9f, 0.3f, 0.0f}, axes, 0.3f);
    close(s.sampling.selection.trajectory_8144.position_at(static_cast<float>(frame) * DT), before);
    CHECK(s.selected_candidate.trajectory == candidate);
    CHECK(s.predictions[0] == prediction);
}

S3_TEST(animation_correction_retains_native_fused_up_plus_forward_operation) {
    auto s = selected();
    const Frame axes{{{0.0f, 0.0f, 1.0f, 0.0f}, {0.8f, 0.6f, 0.0f, 0.0f}, {0.6f, -0.8f, 0.0f, 0.0f}, {}}};
    s.adjust_animation(10, {0.0f, 0.6f, -0.8f, 0.0f}, axes, 0.0f);
    const float forward = 0.6f * -0.8f;
    // Rust `-0.8_f32.mul_add(0.6, forward)` parses as -(0.8.mul_add(0.6, forward)).
    const float expected = -s3::rs::mul_add(0.8f, 0.6f, forward);
    CHECK_NE(expected, -(0.8f * 0.6f + forward));
    CHECK_EQ(s3::rs::to_bits(s.correction_8304[0]), s3::rs::to_bits(expected));
}

S3_TEST(requery_gates_first_hit_changed_hit_and_miss_retention) {
    auto s = selected();
    const auto old = s.sampling.selection;
    CHECK(!s.begin_requery(0x10000000, 0, 0.3f).unwrap().has_value());
    CHECK(s.sampling.restart_allowed_8493);
    auto request = *s.begin_requery(0, 0, 0.3f).unwrap();
    CHECK_EQ(request.trajectory.position, s.selected_candidate.trajectory.position);
    s.complete_requery(Prediction{hit(41, {3.0f, 2.0f, 1.0f, 0.0f}), request}).unwrap();
    CHECK_EQ(s.requery_count_8488, 1);
    CHECK_EQ(s.sampling.selection.position_6176, old.position_6176);
    s.sampling.restart_allowed_8493 = true;
    request = *s.begin_requery(0, 0, 0.3f).unwrap();
    s.complete_requery(Prediction{hit(42, {4.0f, 2.0f, 1.0f, 0.0f}), request}).unwrap();
    CHECK_EQ(s.sampling.selection.landing_frame_8480, 42);
    CHECK(s.sampling.selection.trajectory_8144 == old.trajectory_8144);
    const auto retained = s.sampling.selection;
    s.sampling.restart_allowed_8493 = true;
    request = *s.begin_requery(0, 0, 0.3f).unwrap();
    s.complete_requery(Prediction{QueryResult::miss(), request}).unwrap();
    CHECK_EQ(s.requery_count_8488, 2);
    CHECK(!s.requery_pending_8499);
    CHECK_EQ(s.sampling.selection.position_6176, retained.position_6176);
}

S3_TEST(greatest_plane_root_is_not_limited_to_query_duration) {
    const Trajectory t{{0.0f, 1.0f, 0.0f, 0.0f}, {0.0f, 2.0f, 0.0f, 0.0f}, {0.0f, -2.0f, 0.0f, 0.0f}, 0.1f};
    const float time = s3::rs::unwrap(ledge::plane_time(t, {}, UP));
    CHECK(s3::rs::abs(time - (1.0f + s3::rs::sqrt(2.0f))) < 0.0001f);
    Trajectory tangent = t;
    tangent.position = {0.0f, -1.0f, 0.0f, 0.0f};
    CHECK(s3::rs::abs(s3::rs::unwrap(ledge::plane_time(tangent, {}, UP)) - 1.0f) < 0.0001f);
}

S3_TEST(air_six_lines_consume_even_when_all_miss) {
    const ground_query::Edge edge{Vector3::new_(-1.0f, 0.0f, 0.0f), Vector3::new_(1.0f, 0.0f, 0.0f)};
    const ledge::Adjustment adjustment{edge, {}, selected().selected_candidate.trajectory, 60, 0.3f};
    const auto lines = s3::rs::unwrap(ledge::lines(adjustment, 0.4f));
    CHECK_EQ(lines.size(), 6u);
    CHECK_EQ(lines[5].radius, 0.001f);
    CHECK(ledge::consume_lines(std::vector<std::optional<ground_query::LineHit>>(6)).is_ok());
    CHECK(ledge::consume_lines(std::vector<std::optional<ground_query::LineHit>>(7)).is_err());
}
}  // namespace
