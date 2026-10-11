// Port of skate-core/src/player/offboard/ground_job_tests.rs.
#include <string>

#include "harness.hpp"
#include "s3/core/player/offboard/ground_job.hpp"
#include "s3/core/trigonometry.hpp"

namespace {
using namespace s3::player::offboard::ground_job;
namespace ground_query = s3::player::offboard::ground_query;
namespace ground_entry = s3::player::offboard::ground_entry;
using s3::player::offboard::contact_toolkit::IDENTITY;
using s3::rs::Result;

Vector splat(float v) { return Vector{v, v, v, v}; }

Input input() {
    Input i;
    i.contact = ContactSnapshot{30, ContactPrefix::reset()};
    i.frame = IDENTITY;
    i.previous_state = 500;
    i.third_line_position = std::nullopt;
    i.frames_since_teleport = 20;
    i.processed_position = splat(2.0f);
    i.processed_velocity = splat(60.0f);
    i.controls.state_708 = 0.7f;
    i.controls.state_712 = -0.2f;
    i.controls.state_656 = {1.0f, 0.0f, 2.0f, 0.0f};
    i.collision_displacements = {splat(3.0f), splat(4.0f)};
    i.animation_motion = splat(5.0f);
    i.animation_velocity = splat(6.0f);
    i.requested_duration = 0.8f;
    i.requested_phase = 0.3f;
    i.override_duration = 0.4f;
    i.flags_2472 = 0x10000000;
    i.flags_2476 = 4;
    i.flags_2480 = 0x80;
    i.flags_2484 = 0x20000;
    i.flags_2488 = 0x08000000;
    return i;
}
struct Unit {};
Prepared run(ContactPrefix& contact, float& timer, const Input& input) {
    return prepare<Unit>(contact, timer, input, [](const ConsumeInput& i) {
               return Result<GroundAdjustment, Unit>(ground_query::consume_geometry(i, std::nullopt));
           })
        .unwrap();
}

S3_TEST(ready_copies_whole_prefix_not_processed_flags_or_wheel_support) {
    auto i = input();
    i.contact.prefix = ContactPrefix{{1.0f, 2.0f, 3.0f, 0.0f},
                                     {0.6f, 0.8f, 0.0f, 0.0f},
                                     {splat(7.0f), splat(7.0f), splat(7.0f), splat(7.0f)},
                                     splat(8.0f),
                                     splat(9.0f),
                                     splat(10.0f),
                                     splat(11.0f),
                                     12.0f,
                                     13,
                                     14.0f,
                                     15.0f,
                                     0x31,
                                     16};
    auto retained = ContactPrefix::reset();
    float timer = 0.0f;
    const auto out = run(retained, timer, i);
    CHECK(retained == i.contact.prefix);
    CHECK_EQ(out.job.flags, 0x31u);
    CHECK_EQ(out.job.support_id, 16u);
    CHECK_EQ(out.job.contact_normal, retained.normal);
    CHECK_EQ(out.job.support_frame, retained.support_frame);
    CHECK_EQ(out.job.target_position, retained.target_position);
    CHECK_EQ(out.job.target_normal, retained.target_normal);
    CHECK_EQ(out.job.edge_position, retained.edge_position);
    CHECK_EQ(out.job.edge_normal, retained.edge_normal);
    CHECK_EQ(timer, STEP);
}

S3_TEST(unready_native_third_line_changes_only_three_fields_and_keeps_timer) {
    auto retained = ContactPrefix::reset();
    retained.support_180 = 42;
    retained.distance_172 = 17.0f;
    retained.target_position = splat(9.0f);
    const auto old = retained;
    auto i = input();
    i.contact.readiness = 0;
    i.third_line_position = Vector{0.0f, -2.0f, 0.0f, 0.0f};
    float timer = 0.0f;
    run(retained, timer, i);
    auto expected = old;
    expected.flags_176 = 1;
    expected.position = {0.0f, -2.0f, 0.0f, 0.0f};
    expected.normal = i.frame[1];
    CHECK(retained == expected);
    CHECK_EQ(timer, STEP);
}

S3_TEST(no_third_line_or_previous_501_does_not_invent_support) {
    for (std::uint32_t previous : {500u, 501u}) {
        auto i = input();
        i.contact.readiness = -1;
        i.previous_state = previous;
        if (previous == 501) i.third_line_position = splat(7.0f);
        auto retained = ContactPrefix::reset();
        const auto before = retained;
        float timer = 0.0f;
        run(retained, timer, i);
        CHECK(retained == before);
        CHECK_EQ(timer, STEP);
    }
}

S3_TEST(ready_reset_packet_is_not_treated_as_unready_or_stale_contact) {
    auto i = input();
    i.third_line_position = splat(8.0f);
    auto retained = ContactPrefix::reset();
    retained.flags_176 = 0x39;
    retained.position = splat(9.0f);
    float timer = 2.0f;
    run(retained, timer, i);
    CHECK(retained == ContactPrefix::reset());
    CHECK_EQ(timer, 0.0f);
}

S3_TEST(teleport_signed_guard_and_fixed_step_prediction_match_preupdate) {
    const std::pair<std::uint32_t, bool> cases[] = {{19u, true}, {20u, false}, {UINT32_MAX, true}};
    for (auto [frames, suppressed] : cases) {
        auto i = input();
        i.frames_since_teleport = frames;
        i.contact.prefix.flags_176 = 0x38;
        auto retained = ContactPrefix::reset();
        float timer = 0.0f;
        const auto out = run(retained, timer, i);
        CHECK_EQ(out.job.suppress_minimum, suppressed);
        CHECK_EQ((retained.flags_176 & 8u) == 0, suppressed);
        const float expected = s3::rs::mul_add(60.0f, s3::rs::from_bits(0x3c888889u), 2.0f);
        CHECK_EQ(out.job.animation_position, splat(expected));
        CHECK_EQ(out.job.collision_displacements, i.collision_displacements);
        CHECK_EQ(out.job.requested_phase, 0.3f);
        CHECK_EQ(out.job.override_duration, 0.4f);
        CHECK_EQ(out.job.desired_direction, i.controls.state_656);
        CHECK(out.job.mirrored && out.job.sprint_pressed && out.job.suppress_lean);
        CHECK(out.job.animation_directed && out.job.ignore_obstacle);
    }
}

S3_TEST(geometry_changes_job_normal_without_overwriting_retained_prefix) {
    auto i = input();
    i.contact.prefix.normal = {0.6f, 0.8f, 0.0f, 0.0f};
    i.contact.prefix.flags_176 = 0x30;
    auto retained = ContactPrefix::reset();
    int count = 0;
    float timer = 0.0f;
    const auto out = prepare<Unit>(retained, timer, i, [&](const ConsumeInput& in) {
                         count += 1;
                         CHECK_EQ(in.reach_364, 1.0e10f);
                         ground_query::GroundGeometry g;
                         g.frame = ground_query::Frame::IDENTITY;
                         g.kind = 2;
                         g.flag26 = true;
                         g.flag27 = false;
                         g.flag28 = false;
                         return Result<GroundAdjustment, Unit>(ground_query::consume_geometry(in, g));
                     }).unwrap();
    CHECK_EQ(count, 1);
    CHECK_EQ(retained.normal, (Vector{0.6f, 0.8f, 0.0f, 0.0f}));
    CHECK_EQ(out.job.contact_normal, (Vector{0.0f, 1.0f, 0.0f, 0.0f}));
    CHECK(out.geometry.state_754);
    CHECK(out.job.target_frame_present);
    CHECK(!out.job.edge_active);
}

S3_TEST(geometry_failure_is_not_converted_to_a_ground_job) {
    auto retained = ContactPrefix::reset();
    float timer = 0.0f;
    CHECK(prepare<std::string>(retained, timer, input(), [](const ConsumeInput&) {
              return Result<GroundAdjustment, std::string>(s3::rs::make_err(std::string("real query failed")));
          }).is_err());
}

s3::player::offboard::controller::GroundResult motion() {
    s3::player::offboard::controller::GroundResult r;
    r.physical_frame = IDENTITY;
    r.animation_frame = IDENTITY;
    r.surface_frame = IDENTITY;
    return r;
}

S3_TEST(sync_corrects_only_positive_contact_height_and_preserves_result) {
    const std::pair<float, float> cases[] = {{0.2f, 0.2f}, {-0.2f, 0.0f}};
    for (auto [height, expected] : cases) {
        ground_entry::State state;
        auto contact = ContactPrefix::reset();
        contact.flags_176 = 1;
        contact.position[1] = height;
        const auto result = motion();
        CHECK_EQ(sync_frames(state, contact, result), IDENTITY);
        CHECK_EQ(state.frame_80[3][1], expected);
        CHECK_EQ(result.physical_frame, IDENTITY);
    }
}

S3_TEST(sync_finishes_enter_angle_unwind_before_publishing_animation_axes) {
    ground_entry::State state;
    state.flags_144_to_150[6] = true;
    state.duration_180 = STEP;
    state.angle_172 = 1.0f;
    state.angular_velocity_176 = 3.0f;
    const auto frame = sync_frames(state, ContactPrefix::reset(), motion());
    CHECK(!state.flags_144_to_150[6]);
    CHECK_EQ(state.duration_180, 0.0f);
    const auto [sin, cos] = s3::trigonometry::sin_cos(0.9f);
    CHECK(s3::rs::abs(frame[2][0] - sin) < 1.0e-6f);
    CHECK(s3::rs::abs(frame[2][2] - cos) < 1.0e-6f);
}
}  // namespace
