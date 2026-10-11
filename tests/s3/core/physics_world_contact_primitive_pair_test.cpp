// Port of the inline #[cfg(test)] tests in skate-core/src/physics/world_contact/primitive_pair.rs.
#include "harness.hpp"
#include "s3/core/physics/world_contact.hpp"

namespace {
using s3::math::Basis3;
using s3::math::Vector3;
using s3::physics::collision::Sphere;
using namespace s3::physics::world_contact;

PrimitivePairSettings settings() { return PrimitivePairSettings{0.0f, 0.0f, 0.0f, 0.0f, 0.0f}; }
ContactPrimitive sphere(Vector3 center, float radius) { return ContactPrimitive(Sphere{center, radius}); }
void close(float a, float b) { CHECK_MSG(s3::rs::abs(a - b) < 0.00001f, a << " != " << b); }

S3_TEST(sphere_pair_publishes_both_surfaces_and_solver_normal) {
    const auto a = sphere(Vector3::ZERO, 1.0f);
    const auto b = sphere(Vector3{1.2f, 1.6f, 0.0f}, 1.1f);
    const auto result = s3::rs::unwrap(primitive_pair_contacts(a, b, settings()));
    CHECK_EQ(result.count, std::size_t(1));
    close(result.normal.x, -0.6f);
    close(result.normal.y, -0.8f);
    close(result.points[0].a.x, 0.6f);
    close(result.points[0].a.y, 0.8f);
    close(result.points[0].b.x, 0.54f);
    close(result.points[0].b.y, 0.72f);
    const auto reverse = s3::rs::unwrap(primitive_pair_contacts(b, a, settings()));
    close(reverse.normal.x, -result.normal.x);
    close(reverse.points[0].a.x, result.points[0].b.x);
}

S3_TEST(separated_spheres_keep_padding_contacts_without_inflating_surface_points) {
    const auto a = sphere(Vector3::ZERO, 1.0f);
    const auto b = sphere(Vector3{2.25f, 0.0f, 0.0f}, 1.0f);
    CHECK(!primitive_pair_contacts(a, b, settings()).has_value());
    PrimitivePairSettings s = settings();
    s.padding_a = 0.125f;
    s.padding_b = 0.125f;
    const auto result = s3::rs::unwrap(primitive_pair_contacts(a, b, s));
    close(result.points[0].a.x, 1.0f);
    close(result.points[0].b.x, 1.25f);
}

S3_TEST(box_capsule_pair_supports_reversed_native_feature_dispatch) {
    const ContactPrimitive a(ContactPrimitive::RoundedBox{
        Vector3::ZERO, Basis3{{{{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}}},
        Vector3{1.0f, 1.0f, 1.0f}, 0.1f});
    const ContactPrimitive b(
        ContactPrimitive::Capsule{Vector3{1.4f, 0.0f, 0.0f}, Vector3{0.0f, 1.0f, 0.0f}, 0.5f, 0.5f});
    const auto result = s3::rs::unwrap(primitive_pair_contacts(a, b, settings()));
    CHECK(result.count > 0);
    close(result.normal.x, -1.0f);
    for (std::size_t i = 0; i < result.count; ++i) {
        close(result.points[i].a.x, 1.1f);
        close(result.points[i].b.x, 0.9f);
    }
    const auto reversed = s3::rs::unwrap(primitive_pair_contacts(b, a, settings()));
    close(reversed.normal.x, 1.0f);
    CHECK_EQ(reversed.count, result.count);
}
}  // namespace
