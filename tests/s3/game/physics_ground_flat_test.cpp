// Port of skate-game/src/physics/tests/flat_ground.rs (mounted in physics/ground.rs as flat_tests).
#include <cmath>

#include "harness.hpp"
#include "s3/game/physics/ground.hpp"

using namespace s3::game::physics::ground;

S3_TEST(flat_render_vertices_and_actual_queries_agree_across_entire_surface) {
    const auto all_surfaces = surfaces(Terrain::Flat);
    std::size_t count = 0;
    for (const auto& s : all_surfaces) count += s.size();
    CHECK_EQ(count, std::size_t(1));
    for (const auto& s : all_surfaces) {
        for (const auto& quad : s) {
            for (const auto& vertex : quad) CHECK_EQ(vertex.y, HEIGHT);
        }
    }
    // Test-only material: geometry checks do not exercise contact response.
    const auto w = world(Terrain::Flat, RetailContactMaterial{0.0f, 0.0f, 0.0f});
    CHECK_EQ(w.triangles().size(), std::size_t(2));
    const auto* metadata = w.query_metadata().unwrap();
    CHECK(metadata->static_edges.empty());
    CHECK_EQ(metadata->packed_surfaces.size(), std::size_t(2));
    for (const auto& entry : w.triangles()) {
        for (const auto& vertex : entry.triangle.vertices) CHECK_EQ(vertex.y, HEIGHT);
    }
    // Includes former downhill route, half-pipe, seam and near each boundary.
    for (const float x : {-49.9f, -12.0f, -3.0f, 0.0f, 3.0f, 12.0f, 49.9f}) {
        for (const float z : {-49.9f, -16.0f, -4.0f, 0.0f, 4.0f, 8.0f, 16.0f, 49.9f}) {
            const auto queried = w.query_thin_line(Vector3{x, 2.0f, z}, Vector3{x, -2.0f, z}).unwrap();
            CHECK_MSG(queried.has_value(), "flat surface query");
            const auto& hit = *queried;
            CHECK(std::fabs(hit.geometry.position.y - HEIGHT) < 1.0e-5f);
            CHECK(std::fabs(hit.geometry.normal.x) < 1.0e-6f);
            CHECK(std::fabs(hit.geometry.normal.z) < 1.0e-6f);
            CHECK(std::fabs(hit.geometry.normal.y - 1.0f) < 1.0e-6f);
        }
    }
}
