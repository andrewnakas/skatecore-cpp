// Port of skate-core/src/air/trajectory/grind_surface/tests.rs.
#include <vector>

#include "harness.hpp"
#include "s3/core/air/trajectory/grind_surface.hpp"
#include "s3/core/air/trajectory/grind_surface/orientation.hpp"

namespace {
using namespace s3::air::trajectory::grind_surface;
using s3::air::trajectory::math::UP;
using Hits = std::array<std::optional<ProbeHit>, 7>;

InvestigationInput input() {
    InvestigationInput i;
    i.start = {0.0f, 0.0f, 0.0f, 0.0f};
    i.end = {0.0f, 0.0f, 2.0f, 0.0f};
    i.reference = {4.0f, 5.0f, 1.0f, 0.0f};
    i.optional_probe = std::nullopt;
    i.deck_center_to_truck = 0.243f;
    return i;
}
ProbeHit hit(float y, float fraction, std::uint32_t surface) {
    return ProbeHit{fraction, {0.243f, y, 1.0f, 0.0f}, UP, surface};
}

S3_TEST(seven_descriptors_preserve_native_index_and_world_down_probe) {
    InvestigationInput value = input();
    value.optional_probe = V{2.0f, 3.0f, 4.0f, 0.0f};
    const Investigation plan = s3::rs::unwrap(prepare(value));
    CHECK_EQ(plan.probes.size(), std::size_t(7));
    CHECK_EQ(plan.center, (V{0.0f, 0.0f, 1.0f, 0.0f}));
    CHECK_EQ(plan.probes[0].start, (V{0.09f, 0.04f, 1.0f, 0.0f}));
    CHECK_EQ(plan.probes[4].start, (V{0.0f, 0.04f, 1.0f, 0.0f}));
    CHECK_EQ(plan.probes[5].start, (V{0.09f, 0.09f, 1.0f, 0.0f}));
    CHECK_EQ(s3::rs::to_bits(plan.probes[5].radius), 0x3a83126fu);
    CHECK(plan.probes[6] == (Probe{{2.0f, 3.0f, 4.0f, 0.0f}, {2.0f, -3.0f, 4.0f, 0.0f}, 0.0f}));
    std::vector<std::size_t> called;
    const auto result = investigate<s3::rs::Unit>(value, [&](std::size_t i, Probe) {
        called.push_back(i);
        return s3::rs::Result<std::optional<ProbeHit>, s3::rs::Unit>(std::optional<ProbeHit>{});
    });
    CHECK(result.is_ok());
    CHECK_EQ(called, (std::vector<std::size_t>{0, 1, 2, 3, 4, 5, 6}));
}

S3_TEST(raised_cross_probe_not_center_probe_blocks_geometry) {
    const Investigation plan = s3::rs::unwrap(prepare(input()));
    Hits hits{};
    hits[4] = hit(0.0f, 0.5f, 17);
    GrindSurface result = resolve(plan, hits, 0);
    CHECK(result.kind == GeometryType::ThinRail);
    CHECK_EQ(result.audio_surface, 17u);
    CHECK_EQ(result.physics_surface, 4u);
    hits[5] = hit(0.0f, 0.5f, 17);
    result = resolve(plan, hits, 0);
    CHECK(result.kind == GeometryType::Impossible);
    CHECK_NE(result.flags & GrindSurface::BLOCKED_CROSS_SECTION, 0u);
    CHECK_EQ(result.flags & GrindSurface::INVALID, 0u);
}

S3_TEST(strict_coarse_boundary_high_side_and_surface_codes) {
    const Investigation plan = s3::rs::unwrap(prepare(input()));
    Hits hits{};
    hits[1] = hit(0.0f, 0.5f, 23 | (8 << 7));
    hits[3] = hit(-0.1f, 0.65f, 9);
    const GrindSurface result = resolve(plan, hits, 0);
    CHECK(result.kind == GeometryType::FatRail);
    CHECK_EQ(result.high_side[0], -1.0f);
    CHECK_EQ(result.audio_surface, 23u);
    CHECK_EQ(result.physics_surface, 8u);
    CHECK_NE(result.flags & GrindSurface::STAIR, 0u);
    hits[3]->fraction = s3::rs::from_bits(s3::rs::to_bits(0.65f) - 1);
    CHECK(resolve(plan, hits, 0).kind == GeometryType::Ledge);
}

S3_TEST(curb_requires_two_coarse_normals_and_strict_height_ranges) {
    const Investigation plan = s3::rs::unwrap(prepare(input()));
    Hits hits{};
    hits[0] = hit(0.0f, 0.5f, 17);
    hits[2] = hit(0.0f, 0.5f, 17);
    hits[3] = hit(-0.22f, 0.9f, 17);
    const GrindSurface result = resolve(plan, hits, 0);
    CHECK_NE(result.flags & GrindSurface::CURB, 0u);
    CHECK_EQ(result.audio_surface, 4u);
    CHECK_EQ(result.physics_surface, 2u);
    hits[3]->position[1] = -0.2f;
    CHECK_EQ(resolve(plan, hits, 0).flags & GrindSurface::CURB, 0u);
    hits[3]->position[1] = -0.22f;
    hits[3]->normal = {0.0f, 0.99f, 0.0f, 0.0f};
    CHECK_EQ(resolve(plan, hits, 0).flags & GrindSurface::CURB, 0u);
}

S3_TEST(optional_query_flags_use_seventh_result_and_strict_drop_test) {
    InvestigationInput value = input();
    value.optional_probe = V{0.0f, 2.0f, 1.0f, 0.0f};
    const Investigation plan = s3::rs::unwrap(prepare(value));
    Hits hits{};
    hits[6] = ProbeHit{0.5f, {1.0f, -1.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f}, 0};
    const GrindSurface result = resolve(plan, hits, 0);
    CHECK_NE(result.flags & GrindSurface::OPTIONAL_NORMAL_TEST, 0u);
    CHECK_NE(result.flags & GrindSurface::OPTIONAL_DROP_TEST, 0u);
    hits[6]->position[1] = -0.99f;
    CHECK_EQ(resolve(plan, hits, 0).flags & GrindSurface::OPTIONAL_DROP_TEST, 0u);
}

S3_TEST(output_reuse_preserves_unassigned_bits_and_native_blocked_latch) {
    const Investigation plan = s3::rs::unwrap(prepare(input()));
    const std::uint32_t old = GrindSurface::INVALID | GrindSurface::CURB | GrindSurface::BLOCKED_CROSS_SECTION | 7;
    const GrindSurface result = resolve(plan, Hits{}, old);
    CHECK_EQ(result.flags & 7u, 7u);
    CHECK_NE(result.flags & GrindSurface::BLOCKED_CROSS_SECTION, 0u);
    CHECK_EQ(result.flags & (GrindSurface::INVALID | GrindSurface::CURB), 0u);
}

S3_TEST(vertical_query_uses_native_no_submission_result) {
    InvestigationInput value = input();
    value.end = {0.0f, 2.0f, 0.0f, 0.0f};
    const auto result = s3::rs::Result<GrindSurface, s3::rs::Unit>(
        investigate<s3::rs::Unit>(value, [](std::size_t, Probe) -> s3::rs::Result<std::optional<ProbeHit>, s3::rs::Unit> {
            S3_PANIC("native gate should not submit");
        })).unwrap();
    CHECK_NE(result.flags & GrindSurface::INVALID, 0u);
    CHECK_EQ(result.audio_surface, 3u);
    CHECK_EQ(result.physics_surface, 1u);
    CHECK(result.kind == GeometryType::Impossible);
    CHECK_EQ(result.tilted_upmost_normal[2], -1.0f);
}

S3_TEST(tilted_normal_is_distinct_from_unrotated_upmost) {
    const V rail{0.0f, 0.0f, 1.0f, 0.0f};
    CHECK_EQ(orientation::tilted_normal(rail, UP, {2.0f, 4.0f}), UP);
    const V tilted = orientation::tilted_normal(rail, UP, {0.0f, 0.0f});
    CHECK_EQ(tilted[1], -1.0f);
    CHECK_EQ(tilted[0], 0.0f);
}
}  // namespace
