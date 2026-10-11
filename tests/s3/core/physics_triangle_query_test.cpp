// Port of skate-core/src/physics/tests/triangle_query.rs.
#include "harness.hpp"
#include "s3/core/physics/triangle_query.hpp"

namespace {
using namespace s3::physics::triangle_query;
using s3::rs::abs;

std::array<Vector3, 3> vertices() {
    return {Vector3{0.0f, 0.0f, 0.0f}, Vector3{0.0f, 0.0f, 1.0f}, Vector3{1.0f, 0.0f, 0.0f}};
}

S3_TEST(wheel_segment_preserves_winding_fraction_and_barycentric_coordinates) {
    const auto hit = s3::rs::unwrap(
        thin_triangle(Vector3{0.25f, 0.1f, 0.25f}, Vector3{0.0f, -0.2f, 0.0f}, vertices()));
    CHECK(abs(hit.fraction - 0.5f) < 1e-6f);
    CHECK(abs(hit.position.y) < 1e-7f);
    CHECK(abs(hit.normal.y - 1.0f) < 1e-6f);
    CHECK_EQ(hit.volume_parameter, (std::array<float, 3>{0.25f, 0.25f, 0.0f}));
    CHECK(!thin_triangle(Vector3{0.25f, -0.1f, 0.25f}, Vector3{0.0f, 0.2f, 0.0f}, vertices()).has_value());
    CHECK(!thin_triangle(Vector3{0.75f, 0.1f, 0.75f}, Vector3{0.0f, -0.2f, 0.0f}, vertices()).has_value());
}

S3_TEST(native_tolerance_accepts_small_endpoint_overrun_without_clamping) {
    const Vector3 start{0.25f, 0.200001f, 0.25f};
    const auto hit = s3::rs::unwrap(thin_triangle(start, Vector3{0.0f, -0.2f, 0.0f}, vertices()));
    CHECK(hit.fraction > 1.0f);
    CHECK(!thin_triangle(Vector3{0.25f, 0.201f, 0.25f}, Vector3{0.0f, -0.2f, 0.0f}, vertices()).has_value());
    CHECK(!thin_triangle(start, Vector3::ZERO, vertices()).has_value());
}

std::optional<TriangleLineHit> sweep(Vector3 start, Vector3 delta, float radius, float fatness) {
    TriangleLineHit result{Vector3::ZERO, Vector3::ZERO, 0.0f, {0.0f, 0.0f, 0.0f}};
    if (triangle_segment(result, start, delta, vertices(), radius, fatness)) return result;
    return std::nullopt;
}

S3_TEST(camera_sweep_resolves_face_edges_vertices_and_query_radius_offset) {
    const Vector3 delta{0.0f, -2.0f, 0.0f};
    const auto face = s3::rs::unwrap(sweep(Vector3{0.25f, 1.0f, 0.25f}, delta, 0.1f, 0.0f));
    CHECK(abs(face.fraction - 0.45f) < 1e-6f);
    CHECK(abs(face.position.y) < 1e-6f);
    const auto rounded_face = s3::rs::unwrap(sweep(Vector3{0.25f, 1.0f, 0.25f}, delta, 0.1f, 0.2f));
    CHECK(abs(rounded_face.fraction - 0.35f) < 1e-6f);
    CHECK(abs(rounded_face.position.y - 0.2f) < 1e-6f);
    const auto edge = s3::rs::unwrap(sweep(Vector3{-0.06f, 1.0f, 0.5f}, delta, 0.1f, 0.0f));
    CHECK(abs(edge.fraction - 0.46f) < 1e-5f);
    CHECK(abs(edge.normal.x + 0.6f) < 1e-4f);
    CHECK(abs(edge.normal.y - 0.8f) < 1e-4f);
    CHECK(abs(edge.position.x) < 1e-5f && abs(edge.position.y) < 1e-5f);
    CHECK_EQ(edge.volume_parameter, (std::array<float, 3>{0.5f, 0.0f, 0.0f}));
    const auto corner = s3::rs::unwrap(sweep(Vector3{-0.04f, 1.0f, -0.03f}, delta, 0.1f, 0.0f));
    const float expected = (1.0f - s3::rs::sqrt(0.01f - 0.0025f)) * 0.5f;
    CHECK(abs(corner.fraction - expected) < 1e-5f);
    CHECK(dot(corner.position, corner.position) < 1e-9f);
    CHECK_EQ(corner.volume_parameter, (std::array<float, 3>{0.0f, 0.0f, 0.0f}));
    CHECK(!sweep(Vector3{-0.2f, 1.0f, 0.5f}, delta, 0.1f, 0.0f).has_value());
}

S3_TEST(rounded_query_handles_reverse_face_overlap_and_parallel_motion) {
    const auto reverse =
        s3::rs::unwrap(sweep(Vector3{0.25f, -1.0f, 0.25f}, Vector3{0.0f, 2.0f, 0.0f}, 0.1f, 0.0f));
    CHECK(abs(reverse.fraction - 0.45f) < 1e-6f);
    CHECK(abs(reverse.normal.y + 1.0f) < 1e-6f);
    const auto overlap = s3::rs::unwrap(sweep(Vector3{0.25f, 0.05f, 0.25f}, Vector3::ZERO, 0.1f, 0.0f));
    CHECK_EQ(overlap.fraction, 0.0f);
    CHECK(abs(overlap.volume_parameter[2] - 0.0075f) < 1e-6f);
    const auto parallel =
        s3::rs::unwrap(sweep(Vector3{-1.0f, 0.06f, 0.5f}, Vector3{2.0f, 0.0f, 0.0f}, 0.1f, 0.0f));
    CHECK(abs(parallel.fraction - 0.46f) < 1e-5f);
    CHECK(abs(parallel.normal.x + 0.8f) < 1e-4f);
    CHECK(abs(parallel.normal.y - 0.6f) < 1e-4f);
}
}  // namespace
