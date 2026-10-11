// Port of skate-core/src/physics/grind_contact/tests.rs.
#include "harness.hpp"
#include "s3/core/physics/grind_contact.hpp"
#include "s3/core/physics/grind_contact/control.hpp"
#include "s3/core/physics/grind_contact/families.hpp"

namespace {
using namespace s3::physics::grind_contact;
using s3::point_graph::PointGraph;
namespace admission = s3::physics::grind_contact::admission;
namespace families = s3::physics::grind_contact::families;
namespace control = s3::physics::grind_contact::control;

const std::array<V, 4> BOARD = {V{1.0f, 0.0f, 0.0f, 0.0f}, V{0.0f, 1.0f, 0.0f, 0.0f}, V{0.0f, 0.0f, 1.0f, 0.0f},
                                V{0.0f, 0.0f, 0.0f, 0.0f}};
const PointGraph<4> GRAPH{{0.0f, 0.3f, 0.7f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}};
admission::Admission make_admission(std::uint32_t category, std::uint32_t state) {
    return admission::Admission{category, state, 2.0f, {0.0f, 0.0f, 2.0f, 0.0f}, &GRAPH};
}
Primitive rail(V start, V end) { return Primitive{start, end, 1}; }
const V ZERO4{0.0f, 0.0f, 0.0f, 0.0f};

S3_TEST(one_truck_admission_happens_before_front_rear_selection) {
    const std::vector<Primitive> edges = {rail(ZERO4, {0.5f, 0.0f, 0.8660254f, 0.0f}),
                                          rail(ZERO4, {0.0f, 0.0f, 1.0f, 0.0f})};
    const std::array<std::optional<TruckContact>, 2> hits = {TruckContact{{0.0f, 0.0f, 0.2f, 0.0f}, 0},
                                                             TruckContact{{0.0f, 0.0f, -0.2f, 0.0f}, 1}};
    const auto result = s3::rs::unwrap(
        families::five_o(BOARD, hits, edges, {0.0f, 0.0f, 2.0f, 0.0f}, 0, 0, 0, 1.0f, make_admission(400, 400)));
    CHECK(!result.front);
    CHECK_EQ(result.geometry.primitive, std::size_t(1));
    CHECK(result.entry_kind == admission::EntryKind::ChangeGrind);
}

S3_TEST(tipslide_drop_in_rejects_contact_above_deck_without_descent) {
    const std::vector<Primitive> edges = {rail(ZERO4, {0.0f, 0.0f, 1.0f, 0.0f})};
    const std::optional<TruckContact> hit = TruckContact{{0.0f, 0.01f, 0.2f, 0.0f}, 0};
    admission::Admission input = make_admission(100, 100);
    input.speed = 1.0f;
    input.velocity = {0.0f, 0.1f, 0.0f, 0.0f};
    CHECK(!families::tipslide(BOARD, {hit, std::nullopt}, edges, input.velocity, 100, 0, 0, 0, 0.0f, BOARD[0], input)
               .has_value());
}

S3_TEST(darkslide_uses_its_own_tilt_and_depth_thresholds) {
    const std::vector<Primitive> edges = {rail(ZERO4, {0.0f, 0.0f, 1.0f, 0.0f})};
    const std::array<V, 4> board = {V{-0.6f, -0.8f, 0.0f, 0.0f}, V{0.8f, -0.6f, 0.0f, 0.0f},
                                    V{0.0f, 0.0f, 1.0f, 0.0f}, V{0.056f, -0.042f, 0.0f, 0.0f}};
    const TruckContact hit{ZERO4, 0};
    const auto result =
        families::darkslide(board, hit, edges[0], {0.0f, 0.0f, 2.0f, 0.0f}, 200, 0, make_admission(200, 201));
    CHECK(result.has_value());
    std::array<V, 4> deeper = board;
    deeper[3] = scale(board[1], 0.08f);
    CHECK(!families::darkslide(deeper, hit, edges[0], {0.0f, 0.0f, 2.0f, 0.0f}, 200, 0, make_admission(200, 201))
               .has_value());
}

S3_TEST(one_truck_clamps_match_both_originals_not_zip_reversed_signs) {
    struct Case {
        bool front;
        float tilt, input, expected;
    };
    const Case cases[] = {
        {true, -0.3f, 0.5f, 0.0f},  {true, -0.3f, -0.5f, -0.011249996f}, {false, 0.3f, -0.5f, 0.0f},
        {false, 0.3f, 0.5f, 0.011249996f}, {true, 0.3f, -0.5f, 0.0f}, {true, 0.3f, 0.5f, 0.011249996f},
        {false, -0.3f, 0.5f, 0.0f}, {false, -0.3f, -0.5f, -0.011249996f},
    };
    for (const Case& c : cases) {
        control::Control ctl;
        ctl.update(3, {0.0f, 0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, c.tilt, 0.0f}, c.front, false, false, 0.0f, 0.0f, c.input,
                   0.0f);
        CHECK_MSG(s3::rs::abs(ctl.pitch - c.expected) < 1e-7f,
                  "front=" << c.front << " tilt=" << c.tilt << " input=" << c.input << ": " << ctl.pitch);
    }
}
}  // namespace
