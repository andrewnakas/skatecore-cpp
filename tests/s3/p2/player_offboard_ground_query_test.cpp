// Port of skate-core/src/player/offboard/ground_query/tests.rs.
#include "harness.hpp"
#include "s3/core/player/offboard/ground_query.hpp"

namespace {
using namespace s3::player::offboard::ground_query;

Vector3 v(float x, float y, float z) { return Vector3::new_(x, y, z); }
QueryContext context() { return QueryContext{0, -1}; }
ConsumeInput input() { return ConsumeInput{Frame::IDENTITY, Vector3::ZERO, 0, 1.0f, v(0.1f, 0.9f, 0.0f)}; }
LineHit hit(Vector3 position) { return LineHit{position, v(0.0f, 1.0f, 0.0f), 0.5f, 0}; }

S3_TEST(edge_selection_requires_both_distances_to_improve_in_order) {
    auto search = edge_search(Frame::IDENTITY, context(), v(0.0f, 0.0f, 2.0f), 0);
    std::vector<Edge> edges;
    for (auto p : {v(0.5f, 0.1f, 0.0f), v(0.1f, 0.15f, 0.0f), v(0.6f, 0.05f, 0.0f)}) edges.push_back(Edge{p, p});
    auto selected = s3::rs::unwrap(select_edge(search, edges));
    CHECK_EQ(selected.closest, edges[0].start);
    auto reversed = s3::rs::unwrap(select_edge(search, {edges[1], edges[0]}));
    CHECK_EQ(reversed.closest, edges[1].start);
}

S3_TEST(biped_packet_has_seventh_downward_line_and_rounded_sixth_line) {
    EdgeSelection selected{Edge{v(0.0f, 0.0f, -1.0f), v(0.0f, 0.0f, 1.0f)}, Vector3::ZERO};
    auto packet = s3::rs::unwrap(prepare_packet(Frame::IDENTITY, context(), selected, 0.25f));
    CHECK_EQ(packet.lines[0].start, v(0.09f, 0.04f, 0.0f));
    CHECK_EQ(packet.lines[5].radius, 0.001f);
    CHECK_EQ(packet.lines[6].start, v(0.0f, 0.0f, 0.2f));
    CHECK_EQ(packet.lines[6].end, v(0.0f, -6.0f, 0.2f));
    CHECK_EQ(int(GroundQueryPacket::SOURCE_POOL_MASK), 7);
}

S3_TEST(no_query_preserves_existing_contact_up_but_contact_fallback_runs) {
    auto i = input();
    auto out = consume_geometry(i, std::nullopt);
    CHECK(!out.state_753);
    CHECK_EQ(out.input_up_416, i.previous_input_up_416);
    auto i1 = i;
    i1.contact_flags_368 = 1;
    out = consume_geometry(i1, std::nullopt);
    CHECK(out.state_753);
    CHECK_EQ(out.input_up_416, v(0.0f, 1.0f, 0.0f));
    auto i9 = i;
    i9.contact_flags_368 = 9;
    out = consume_geometry(i9, std::nullopt);
    CHECK(!out.state_753);
}

S3_TEST(reach_and_material_flag_gate_step_without_gating_side_contact) {
    GroundGeometry g{Frame::IDENTITY, 2, true, false, false};
    auto i = input();
    i.reach_364 = 0.7f;
    auto out = consume_geometry(i, g);
    CHECK(!out.state_752);
    CHECK(!out.state_754);
    out = consume_geometry(input(), g);
    CHECK(out.state_752 && out.state_754);
    auto g28 = g;
    g28.flag28 = true;
    out = consume_geometry(input(), g28);
    CHECK(!out.state_752);
    auto g1 = g28;
    g1.kind = 1;
    out = consume_geometry(input(), g1);
    CHECK(out.state_753);
}

S3_TEST(seventh_hit_height_and_first_hit_material_reach_consumed_flags) {
    auto packet = s3::rs::unwrap(prepare_packet(
        Frame::IDENTITY, context(), EdgeSelection{Edge{v(0.0f, 0.0f, -1.0f), v(0.0f, 0.0f, 1.0f)}, Vector3::ZERO},
        0.25f));
    std::array<std::optional<LineHit>, 7> hits{};
    auto h0 = hit(Vector3::ZERO);
    h0.packed_surface = 0x400;
    hits[0] = h0;
    hits[2] = hit(Vector3::ZERO);
    hits[6] = hit(v(0.0f, -1.0f, 0.0f));
    auto g = interpret_hits(packet, hits);
    CHECK_EQ(g.kind, 2u);
    CHECK(g.flag28 && g.flag26);
    CHECK(!g.flag27);
    hits[6] = hit(v(0.0f, -0.9f, 0.0f));
    CHECK(!interpret_hits(packet, hits).flag26);
}
}  // namespace
