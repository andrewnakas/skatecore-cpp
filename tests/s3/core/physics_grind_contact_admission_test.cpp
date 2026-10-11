// Port of skate-core/src/physics/grind_contact/admission_tests.rs.
#include "harness.hpp"
#include "s3/core/physics/grind_contact.hpp"

namespace {
using namespace s3::physics::grind_contact;
using namespace s3::physics::grind_contact::admission;
using s3::point_graph::PointGraph;

const PointGraph<4> GRAPH{{0.0f, 0.3f, 0.7f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}};
const V RAIL{1.0f, 0.0f, 0.0f, 0.0f};
Admission context(std::uint32_t category, std::uint32_t state, V velocity) {
    return Admission{category, state, 2.0f, velocity, &GRAPH};
}

S3_TEST(current_grind_and_family_change_have_different_angle_gates) {
    const Admission input = context(400, 401, {0.8660254f, 0.0f, 0.5f, 0.0f});
    CHECK(input.test(401, RAIL, false) == (Decision{EntryKind::StayInGrind, true}));
    CHECK(input.test(403, RAIL, true) == (Decision{EntryKind::ChangeGrind, false}));
    Admission backslash = input;
    backslash.state = 404;
    CHECK(backslash.test(402, RAIL, true).kind == EntryKind::StayInGrind);
}

S3_TEST(ground_entries_distinguish_above_below_coping_and_drop_in) {
    const Admission above = context(100, 100, {2.0f, -1.0f, 0.0f, 0.0f});
    CHECK(above.test(401, RAIL, false).kind == EntryKind::RideFromAbove);
    const Admission below = context(100, 100, {2.0f, 1.0f, 1.0f, 0.0f});
    CHECK(below.test(401, RAIL, false).kind == EntryKind::RideFromBelow);
    const Admission coping = context(100, 100, {2.0f, 1.0f, 0.0f, 0.0f});
    CHECK(coping.test(401, RAIL, false).kind == EntryKind::RideIntoCoping);
    Admission drop_in = above;
    drop_in.speed = 1.0f;
    CHECK(drop_in.test(403, RAIL, true) == (Decision{EntryKind::DropIn, true}));
    CHECK(drop_in.test(401, RAIL, false).kind == EntryKind::RideFromAbove);
    Admission boundary = drop_in;
    boundary.speed = 1.45f;
    CHECK(boundary.test(403, RAIL, true).kind != EntryKind::DropIn);
}

S3_TEST(coping_admission_uses_cross_rail_speed_and_the_authored_graph) {
    const Admission slow = context(100, 100, {1.0f, 1.0f, 0.0f, 0.0f});
    CHECK(slow.test(401, RAIL, false).allowed);
    const Admission fast = context(100, 100, {1.0f, 10.0f, 0.0f, 0.0f});
    CHECK(!fast.test(401, RAIL, false).allowed);
    const PointGraph<4> zero{GRAPH.x, {0.0f, 0.0f, 0.0f, 0.0f}};
    Admission z = slow;
    z.threshold_vs_slope = &zero;
    CHECK(!z.test(401, RAIL, false).allowed);
}

S3_TEST(airborne_admission_and_projection_keep_the_original_leaf_boundary) {
    CHECK(context(200, 201, {0.0f, 0.0f, 8.0f, 0.0f}).test(401, RAIL, false) == (Decision{EntryKind::AirToGrind, true}));
    CHECK(context(100, 100, {0.0f, -8.0f, 0.0f, 0.0f}).test(401, RAIL, false).allowed);
    CHECK_EQ(approach_slope_sine(RAIL, {3.0f, 0.0f, 0.0f, 0.0f}), 0.0f);
    CHECK_EQ(approach_slope_sine(RAIL, {3.0f, 0.01f, 0.0f, 0.0f}), 0.0f);
}
}  // namespace
