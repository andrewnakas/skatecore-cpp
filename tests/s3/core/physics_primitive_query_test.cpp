// Port of skate-core/src/physics/tests/primitive_query.rs.
#include <algorithm>
#include <vector>

#include "harness.hpp"
#include "s3/core/physics/collision.hpp"
#include "s3/core/physics/world_contact.hpp"

namespace {
using s3::math::Basis3;
using s3::math::Vector3;
using namespace s3::physics::collision;
using s3::physics::world_contact::ContactPrimitive;
using s3::physics::world_contact::primitive_triangle_world_contacts;
using s3::physics::world_contact::transform_triangle_volume;
using s3::physics::world_contact::triangle_from_volume;
namespace consts = s3::rs::f32::consts;

Vector3 v(float x, float y, float z) { return Vector3{x, y, z}; }
Triangle floor_() {
    return triangle_from_volume({v(-5.0f, 0.0f, -5.0f), v(0.0f, 0.0f, 5.0f), v(5.0f, 0.0f, -5.0f)}, 0.0f,
                                {-1.0f, -1.0f, -1.0f}, 2);
}
WorldContactSettings settings(float padding) { return WorldContactSettings{padding, 1.0f, -1.0f, 0.0f, false}; }
void close(float a, float b) { CHECK_MSG(s3::rs::abs(a - b) < 2.0e-5f, a << " != " << b); }
void same(Vector3 a, Vector3 b) {
    close(a.x, b.x);
    close(a.y, b.y);
    close(a.z, b.z);
}
Basis3 identity() { return Basis3{{{{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}}}; }

S3_TEST(volume_builder_uses_native_winding_edges_and_flags) {
    const Triangle triangle = triangle_from_volume({v(0.0f, 0.0f, 0.0f), v(0.0f, 0.0f, 4.0f), v(3.0f, 0.0f, 0.0f)},
                                                   0.02f, {1.0f, -1.0f, 1.0f}, 0x3e3);
    same(triangle.feature.normal, v(0.0f, 1.0f, 0.0f));
    same(triangle.feature.edges[0], v(1.0f, 0.0f, 0.0f));
    same(triangle.feature.edges[1], v(-0.6f, 0.0f, 0.8f));
    same(triangle.feature.edges[2], v(0.0f, 0.0f, -1.0f));
    const float expected[3] = {3.0f, 5.0f, 4.0f};
    for (std::size_t i = 0; i < 3; ++i) close(triangle.edge_lengths[i], expected[i]);
    CHECK_EQ(triangle.feature.flags, 0x3e1u);
    CHECK_EQ(triangle.feature.edge_cosines, (std::array<float, 3>{1.0f, -1.0f, 1.0f}));
    CHECK_EQ(triangle.fatness, 0.02f);
}

S3_TEST(volume_transform_keeps_local_normal_and_rebuilds_world_edges) {
    const Triangle triangle = transform_triangle_volume(
        {v(0.0f, 0.0f, 0.0f), v(0.0f, 0.0f, 4.0f), v(3.0f, 0.0f, 0.0f)}, 0.02f, {-1.0f, -1.0f, -1.0f}, 2,
        Basis3{{{{0.0f, 1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}}}, v(4.0f, 5.0f, 6.0f));
    same(triangle.feature.normal, v(-1.0f, 0.0f, 0.0f));
    same(triangle.vertices[2], v(4.0f, 8.0f, 6.0f));
    same(triangle.feature.edges[0], v(0.0f, 1.0f, 0.0f));
    close(triangle.edge_lengths[1], 5.0f);
}

S3_TEST(generic_sphere_world_path_matches_existing_independent_sphere_branch) {
    const Sphere sphere{v(0.4f, 0.13f, -0.2f), 0.15f};
    Triangle triangle = floor_();
    triangle.fatness = 0.01f;
    const auto generic = s3::rs::unwrap(
        primitive_triangle_world_contacts(ContactPrimitive(sphere), triangle, Vector3::ZERO, settings(0.0f)));
    const auto previous =
        s3::rs::unwrap(sphere_triangle_world_contact(sphere, triangle, Vector3::ZERO, settings(0.0f)));
    CHECK_EQ(generic.count, std::size_t(1));
    same(generic.normal, previous.normal);
    same(generic.points[0].a, previous.points.a);
    same(generic.points[0].b, previous.points.b);
    same(generic.normal, v(0.0f, 1.0f, 0.0f));
    close(generic.points[0].a.y, -0.02f);
    close(generic.points[0].b.y, 0.01f);
}

S3_TEST(capsule_face_keeps_both_axial_end_contacts) {
    const ContactPrimitive capsule(
        ContactPrimitive::Capsule{v(0.0f, 0.1f, 0.0f), v(1.0f, 0.0f, 0.0f), 0.4f, 0.15f});
    const auto result =
        s3::rs::unwrap(primitive_triangle_world_contacts(capsule, floor_(), Vector3::ZERO, settings(0.0f)));
    CHECK_EQ(result.count, std::size_t(2));
    same(result.normal, v(0.0f, 1.0f, 0.0f));
    std::vector<float> xs{result.points[0].a.x, result.points[1].a.x};
    std::sort(xs.begin(), xs.end(), [](float a, float b) { return s3::rs::total_cmp(a, b) < 0; });
    close(xs[0], -0.4f);
    close(xs[1], 0.4f);
    for (std::size_t i = 0; i < 2; ++i) {
        const auto& p = result.points[i];
        close(p.a.y, -0.05f);
        close(p.b.y, 0.0f);
        close(p.a.x, p.b.x);
    }
}

S3_TEST(rounded_box_face_produces_all_four_corner_contacts) {
    const ContactPrimitive shape(
        ContactPrimitive::RoundedBox{v(0.0f, 0.09f, 0.0f), identity(), v(0.4f, 0.08f, 0.2f), 0.02f});
    const auto result =
        s3::rs::unwrap(primitive_triangle_world_contacts(shape, floor_(), Vector3::ZERO, settings(0.0f)));
    CHECK_EQ(result.count, std::size_t(4));
    same(result.normal, v(0.0f, 1.0f, 0.0f));
    for (std::size_t i = 0; i < result.count; ++i) {
        const auto& p = result.points[i];
        close(p.a.y, -0.01f);
        close(p.b.y, 0.0f);
        close(s3::rs::abs(p.a.x), 0.4f);
        close(s3::rs::abs(p.a.z), 0.2f);
    }
}

S3_TEST(moving_triangle_face_preserves_three_vertex_contacts) {
    const Triangle moving = triangle_from_volume(
        {v(-0.4f, 0.01f, -0.2f), v(0.0f, 0.01f, 0.2f), v(0.4f, 0.01f, -0.2f)}, 0.02f, {-1.0f, -1.0f, -1.0f}, 2);
    const auto result = s3::rs::unwrap(
        primitive_triangle_world_contacts(ContactPrimitive(moving), floor_(), Vector3::ZERO, settings(0.0f)));
    CHECK_EQ(result.count, std::size_t(3));
    same(result.normal, v(0.0f, 1.0f, 0.0f));
    for (std::size_t i = 0; i < 3; ++i) {
        close(result.points[i].a.y, -0.01f);
        close(result.points[i].b.y, 0.0f);
    }
}

S3_TEST(prediction_uses_fixed_sixtieth_and_separate_volume_padding) {
    const ContactPrimitive sphere(Sphere{v(0.0f, 0.2f, 0.0f), 0.1f});
    CHECK(!primitive_triangle_world_contacts(sphere, floor_(), Vector3::ZERO, settings(0.0f)).has_value());
    const auto predicted =
        s3::rs::unwrap(primitive_triangle_world_contacts(sphere, floor_(), v(0.0f, -6.0f, 0.0f), settings(0.001f)));
    close(predicted.points[0].a.y, 0.1f);
    CHECK(!primitive_triangle_world_contacts(sphere, floor_(), v(0.0f, 6.0f, 0.0f), settings(0.001f)).has_value());
}

S3_TEST(one_sided_world_triangle_rejects_below_surface) {
    Triangle triangle = floor_();
    triangle.feature.flags |= TriangleFeature::ONE_SIDED;
    const ContactPrimitive below(Sphere{v(0.0f, -0.05f, 0.0f), 0.1f});
    CHECK(!primitive_triangle_world_contacts(below, triangle, Vector3::ZERO, settings(0.0f)).has_value());
    const ContactPrimitive above(Sphere{v(0.0f, 0.05f, 0.0f), 0.1f});
    CHECK(primitive_triangle_world_contacts(above, triangle, Vector3::ZERO, settings(0.0f)).has_value());
}

S3_TEST(triangle_box_edge_cross_axis_rejects_despite_overlapping_face_intervals) {
    // All three box-axis intervals and the triangle normal overlap. The
    // diagonal triangle edge x+z=2.1 is sqrt(0.005) beyond box corner(1,1),
    // so this separation can only be found by an edge-cross SAT direction.
    const Triangle triangle = triangle_from_volume(
        {v(0.0f, 0.0f, 2.1f), v(3.0f, 0.0f, 3.0f), v(2.1f, 0.0f, 0.0f)}, 0.0f, {-1.0f, -1.0f, -1.0f}, 0xe2);
    auto shape = [](float radius) {
        return ContactPrimitive(
            ContactPrimitive::RoundedBox{Vector3::ZERO, identity(), v(1.0f, 1.0f, 1.0f), radius});
    };
    CHECK(!primitive_triangle_world_contacts(shape(0.05f), triangle, Vector3::ZERO, settings(0.0f)).has_value());
    const auto result =
        s3::rs::unwrap(primitive_triangle_world_contacts(shape(0.08f), triangle, Vector3::ZERO, settings(0.0f)));
    CHECK_EQ(result.count, std::size_t(1));
    same(result.normal, v(-consts::FRAC_1_SQRT_2, 0.0f, -consts::FRAC_1_SQRT_2));
    same(result.points[0].b, v(1.05f, 0.0f, 1.05f));
    const float offset = 0.08f * consts::FRAC_1_SQRT_2;
    same(result.points[0].a, v(1.0f + offset, 0.0f, 1.0f + offset));
}

S3_TEST(rotated_box_and_triangle_keep_the_plane_manifold) {
    const float c = 0.8660254f;
    const Basis3 basis{{{{c, 0.5f, 0.0f}, {-0.5f, c, 0.0f}, {0.0f, 0.0f, 1.0f}}}};
    const Triangle triangle =
        transform_triangle_volume(floor_().vertices, 0.0f, {-1.0f, -1.0f, -1.0f}, 2, basis, v(2.0f, 3.0f, 4.0f));
    const ContactPrimitive shape(ContactPrimitive::RoundedBox{v(2.0f - 0.09f * 0.5f, 3.0f + 0.09f * c, 4.0f), basis,
                                                              v(0.4f, 0.08f, 0.2f), 0.02f});
    const auto result =
        s3::rs::unwrap(primitive_triangle_world_contacts(shape, triangle, Vector3::ZERO, settings(0.0f)));
    CHECK_EQ(result.count, std::size_t(4));
    same(result.normal, v(-0.5f, c, 0.0f));
    for (std::size_t i = 0; i < result.count; ++i) {
        const auto& p = result.points[i];
        close((p.a.x - 2.0f) * -0.5f + (p.a.y - 3.0f) * c, -0.01f);
        close((p.b.x - 2.0f) * -0.5f + (p.b.y - 3.0f) * c, 0.0f);
    }
}
}  // namespace
