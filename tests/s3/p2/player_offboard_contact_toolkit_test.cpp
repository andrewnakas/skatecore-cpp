// Port of skate-core/src/player/offboard/contact_toolkit/tests.rs.
#include <cmath>

#include "harness.hpp"
#include "s3/core/player/offboard/contact_toolkit.hpp"
#include "s3/core/player/offboard/contact_toolkit/analyzer_math.hpp"
#include "s3/core/player/offboard/contact_toolkit/stages.hpp"

namespace {
using namespace s3::player::offboard::contact_toolkit;
using s3::air::trajectory::QueryResult;

Input input() {
    return Input::from_vectors({ZERO, Vector{0.0f, 0.0f, 1.0f, 0.0f}, UP, Vector{1.0f, 0.0f, 0.0f, 0.0f}, ZERO, UP,
                                Vector{1.0f, 0.0f, 0.0f, 0.0f}});
}
struct EmptyScene {
    using Error = s3::rs::Unit;
    s3::rs::Result<QueryResults, Error> execute(const Batch& batch) const {
        QueryResults r;
        r.trajectories = {QueryResult::miss(), QueryResult::miss(), QueryResult::miss()};
        r.lines.assign(batch.lines.size(), std::nullopt);
        return r;
    }
};

S3_TEST(empty_world_refresh_cannot_publish_walkable_support) {
    Owner owner;
    CHECK(!owner.refresh().has_value());
    CHECK(owner.submit(input(), -1, EmptyScene{}).is_ok());
    CHECK(owner.has_pending());
    auto result = s3::rs::unwrap(owner.refresh());
    CHECK_EQ(result.prefix.flags_176 & 3u, 0u);
    CHECK_EQ(result.prefix.support_180, 0u);
    CHECK_EQ(result.prefix.distance_172, 1e10f);
    CHECK(!owner.has_pending());
    CHECK(!owner.refresh().has_value());
}

S3_TEST(classification_requires_three_prior_matches_and_flat_clears_history) {
    classification::History history{};
    auto prefix = ContactPrefix::reset();
    prefix.flags_176 = 0x40;
    for (int k = 0; k < 3; ++k) {
        history.classify(prefix, {0.0f, 0.3f, 1.0f, 0.0f});
        CHECK_EQ(prefix.kind_164, 0u);
    }
    history.classify(prefix, {0.0f, 0.3f, 1.0f, 0.0f});
    CHECK_EQ(prefix.kind_164, 1u);
    history.classify(prefix, {0.0f, 0.0f, 1.0f, 0.0f});
    CHECK_EQ(prefix.kind_164, 0u);
    history.classify(prefix, {0.0f, 0.3f, 1.0f, 0.0f});
    CHECK_EQ(prefix.kind_164, 0u);
}

S3_TEST(source_tangent_polynomial_has_reasonable_stock_angle_outputs) {
    for (float degrees : {6.0f, 15.0f, 30.0f, 32.0f, 35.0f, 50.0f}) {
        const float angle = degrees * 0.017453292f;
        CHECK(std::fabs(analyzer_math::tangent(angle) - std::tan(angle)) < 0.00001f);
    }
}

S3_TEST(secondary_edge_is_selected_only_when_higher_than_real_line_contact) {
    const auto layout = ProbeLayout::stock();
    const auto batch = layout.prepare(input(), -1);
    auto result = EmptyScene{}.execute(batch).unwrap();
    result.edges.push_back({Vector{0.0f, 0.1f, -1.0f, 0.0f}, Vector{0.0f, 0.1f, 2.0f, 0.0f}});
    const auto descriptor = batch.secondary[0];
    auto position = descriptor.line.start;
    position[1] = 0.2f;
    result.lines[descriptor.forward_index] = LineHit{position, UP, 0.375f, 17, IDENTITY, 73};
    const auto samples = collection::collect(batch, layout, result);
    CHECK_EQ(samples.ground[0].position, position);
    for (std::size_t k = 1; k < samples.ground.size(); ++k) {
        CHECK(std::fabs(samples.ground[k].position[1] - 0.1f) < 1e-6f);
    }
}

S3_TEST(stock_probe_counts_order_and_reverse_indices) {
    const auto layout = ProbeLayout::stock();
    CHECK_EQ(layout.secondary.size(), std::size_t(23));
    CHECK_EQ(layout.primary.size(), std::size_t(15));
    const auto batch = layout.prepare(input(), 27);
    CHECK_EQ(batch.lines.size(), std::size_t(44));
    CHECK_EQ(batch.matching_group, 27);
    CHECK_EQ(batch.mesh_reject_mask, 0x6000u);
    for (const auto& d : batch.primary) {
        CHECK(batch.lines[d.forward_index] == d.line);
        if (d.reverse_index) {
            CHECK_EQ(batch.lines[*d.reverse_index].start, d.line.end);
            CHECK_EQ(batch.lines[*d.reverse_index].end, d.line.start);
        }
    }
    CHECK_EQ(batch.trajectories[0].trajectory.position, (Vector{0.0f, 1.2f, 0.0f, 0.0f}));
    CHECK_EQ(batch.trajectories[0].trajectory.velocity, (Vector{0.0f, -2.0f, 0.0f, 0.0f}));
    CHECK_EQ(batch.trajectories[0].radius, 0.01f);
}

S3_TEST(side_probes_follow_animation_basis_but_centre_follows_surface) {
    auto i = input();
    i.animation_right = {0.0f, 0.0f, -1.0f, 0.0f};
    const auto batch = ProbeLayout::stock().prepare(i, -1);
    CHECK_EQ(batch.trajectories[0].trajectory.position, (Vector{0.0f, 1.2f, 0.0f, 0.0f}));
    CHECK_EQ(batch.trajectories[1].trajectory.position, (Vector{0.0f, 0.8f, 0.12f, 0.0f}));
    CHECK_EQ(batch.trajectories[2].trajectory.position, (Vector{0.0f, 0.8f, -0.12f, 0.0f}));
}

S3_TEST(prefix_reset_does_not_invent_support_or_reach) {
    const auto p = ContactPrefix::reset();
    CHECK_EQ(p.flags_176, 0u);
    CHECK_EQ(p.kind_164, 0u);
    CHECK_EQ(p.distance_168, 1e10f);
    CHECK_EQ(p.distance_172, 1e10f);
    CHECK_EQ(p.edge_normal, ZERO);
}

S3_TEST(support_uses_refined_normal_and_mesh_frame_and_strict_side_window) {
    std::array<QueryResult, 3> hits{QueryResult::miss(), QueryResult::miss(), QueryResult::miss()};
    hits[0].contact_time = 0.0f;
    hits[0].contact_position = {0.0f, -0.11f, 0.0f, 0.0f};
    hits[0].geometry = 92;
    hits[1].contact_time = 0.0f;
    hits[1].contact_position = {0.0f, 0.2f, 0.0f, 0.0f};
    auto p = ContactPrefix::reset();
    p.consume_support(input(), hits, 0x40);
    CHECK_EQ(p.flags_176, 0x49u);
    CHECK_EQ(p.support_180, 92u);
    hits[1].contact_position[1] = 0.199f;
    p = ContactPrefix::reset();
    p.consume_support(input(), hits, 0);
    CHECK_EQ(p.flags_176, 0x19u);
}

S3_TEST(projected_contacts_are_bounded_and_reject_behind_obstacles) {
    Samples samples;
    CHECK(!samples.insert(input(), {0.0f, 0.0f, -1.0f, 0.0f}, UP, 0, -1.0f, 0));
    for (int n = 0; n < 64; ++n) {
        CHECK(samples.insert(input(), {0.0f, 0.0f, float(n), 0.0f}, UP, 1, -1.0f, 0));
    }
    CHECK(!samples.insert(input(), ZERO, UP, 1, -1.0f, 0));
    CHECK_EQ(samples.original_ground_count, std::size_t(64));
}

S3_TEST(sweep_contact_time_is_not_the_line_fraction) {
    const LineProbe p{{0.0f, 1.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f, 0.0f}, 0.01f};
    const LineHit hit{ZERO, UP, 0.495f, 0, IDENTITY, 7};
    using E = s3::rs::Unit;
    auto r = query_sweep<E>(
                 p, [&](LineProbe) { return s3::rs::Result<std::optional<LineHit>, E>(std::optional<LineHit>(hit)); },
                 [](Vector, float) {
                     return s3::rs::Result<std::vector<std::array<Vector, 3>>, E>(std::vector<std::array<Vector, 3>>{});
                 })
                 .unwrap();
    CHECK(std::fabs(r.contact_time - 0.5f / 60.0f) < 1e-7f);
    CHECK_EQ(r.contact_frame, 0);
    CHECK_EQ(r.geometry, 7u);
}
}  // namespace
