// Port of skate-core/src/physics/tests/deck_geometry.rs.
#include <vector>

#include "harness.hpp"
#include "s3/core/physics/mass.hpp"

namespace {
using namespace s3::physics::mass;
using s3::physics::drive_frames::RetailAffineTransform;

void close(float actual, float expected, float tolerance) {
    CHECK_MSG(s3::rs::abs(actual - expected) <= tolerance, actual << " != " << expected);
}

template <class T>
const T& expect_shape(const DeckShape& shape) {
    const T* v = std::get_if<T>(&shape.value);
    if (!v) S3_PANIC("explicit panic");
    return *v;
}

S3_TEST(stock_deck_has_actual_child_order_dimensions_and_source_flags) {
    const DeckGeometry deck = DeckGeometry::new_(DeckGeometrySettings::STOCK);
    CHECK_EQ(deck.children.size(), std::size_t(15));
    const auto& box = expect_shape<DeckShape::RoundedBox>(deck.children[0].shape);
    const auto half_extents = box.half_extents;
    const float radius = box.radius;
    close(radius, 0.00675f, 1e-9f);
    close(half_extents.x + radius, 0.12f, 1e-8f);
    close(half_extents.y + radius, 0.0075f, 1e-9f);
    close(half_extents.z + radius, 0.295f, 3e-8f);
    const std::pair<std::size_t, float> capsules[] = {{1, 0.1125f}, {2, -0.1125f}};
    for (const auto& [index, x] : capsules) {
        const auto& capsule = expect_shape<DeckShape::Capsule>(deck.children[index].shape);
        close(capsule.radius, 0.0075f, 1e-9f);
        close(capsule.half_length, 0.295f, 3e-8f);
        close(deck.children[index].transform.translation.x, x, 1e-8f);
        CHECK_EQ(deck.children[index].transform.basis, RetailAffineTransform::IDENTITY.basis);
    }
    const std::pair<std::size_t, float> spheres[] = {{3, 0.235f}, {4, -0.235f}};
    for (const auto& [index, z] : spheres) {
        CHECK(deck.children[index].shape == DeckShape(DeckShape::Sphere{0.035f}));
        CHECK_EQ(deck.children[index].transform.translation, (Vector3{0.0f, -0.02f, z}));
    }
    for (std::size_t index = 5; index < deck.children.size(); ++index) {
        const auto& t = expect_shape<DeckShape::Triangle>(deck.children[index].shape);
        CHECK_EQ(t.fatness, 0.0075f);
        CHECK_EQ(t.edge_cosines, (std::array<float, 3>{1.0f, -1.0f, 1.0f}));
        CHECK_EQ(t.volume_flags, 0x3E2u);
        CHECK_EQ(t.vertices[0], (Vector3{0.0f, 0.0f, index % 2 == 1 ? -0.295f : 0.295f}));
    }
}

S3_TEST(fans_join_at_shared_vertices_and_use_independent_end_angles) {
    const DeckGeometry deck = DeckGeometry::new_(DeckGeometrySettings::STOCK);
    for (std::size_t side : {0, 1}) {
        std::vector<std::array<Vector3, 3>> triangles;
        for (std::size_t i = 5 + side; i < deck.children.size(); i += 2) {
            triangles.push_back(expect_shape<DeckShape::Triangle>(deck.children[i].shape).vertices);
        }
        for (std::size_t i = 0; i + 1 < triangles.size(); ++i) {
            CHECK_EQ(triangles[i][2], triangles[i + 1][1]);
        }
        close(triangles[0][1].x, 0.1125f, 1e-8f);
        close(triangles[4][2].x, -0.1125f, 2e-7f);
        for (const auto& vertices : triangles) {
            CHECK(vertices[2].y >= -1e-7f);
            const float from_center = s3::rs::abs(vertices[2].z - vertices[0].z);
            if (from_center > 0.01f) {
                const float actual_angle = s3::rs::to_degrees(s3::rs::atan(vertices[2].y / from_center));
                close(actual_angle, side == 0 ? 12.5f : 13.0f, 0.0001f);
            }
        }
    }
}

S3_TEST(collision_toggles_do_not_remove_mass_and_spheres_remain_enabled) {
    const DeckGeometry stock = DeckGeometry::new_(DeckGeometrySettings::STOCK);
    DeckGeometrySettings settings = DeckGeometrySettings::STOCK;
    settings.enable_deck_volume_collisions = false;
    settings.enable_end_volume_collisions = false;
    const DeckGeometry disabled = DeckGeometry::new_(settings);
    CHECK_EQ(stock.mass_moments(), disabled.mass_moments());
    for (std::size_t index = 0; index < disabled.children.size(); ++index) {
        CHECK_EQ(disabled.children[index].collision_enabled, index == 3 || index == 4);
    }
}

S3_TEST(triangle_mass_uses_source_fat_aabb_instead_of_triangle_area) {
    const DeckChild child{
        DeckShape::Triangle{
            {Vector3{-2.0f, 1.0f, -1.0f}, Vector3{3.0f, 1.0f, -1.0f}, Vector3{1.0f, 1.0f, 5.0f}},
            0.5f,
            {1.0f, -1.0f, 1.0f},
            0x3E2u,
        },
        RetailAffineTransform::IDENTITY,
        true,
    };
    MassMoments moments = child.mass_moments();
    const auto properties = moments.principal_properties();
    // Its fat AABB is [-2.5,0.5,-1.5]..[3.5,1.5,5.5]. The source rounded-box
    // minimum radius is tiny but nonzero; allow its documented numerical floor.
    close(properties.volume, 42.0f, 0.0001f);
    close(properties.local_mass_frame.translation.x, 0.5f, 2e-6f);
    close(properties.local_mass_frame.translation.y, 1.0f, 2e-6f);
    close(properties.local_mass_frame.translation.z, 2.0f, 2e-6f);
}

S3_TEST(stock_mass_is_computed_from_children_and_changes_with_authored_dimensions) {
    const DeckGeometry stock = DeckGeometry::new_(DeckGeometrySettings::STOCK);
    MassMoments moments = stock.mass_moments();
    CHECK_NE(moments, MassMoments::ZERO);
    const auto properties = moments.principal_properties();
    CHECK(properties.volume > 0.0f);
    close(properties.local_mass_frame.translation.x, 0.0f, 1e-6f);
    CHECK(s3::rs::is_finite(properties.local_mass_frame.translation.y));
    const auto inertia = properties.moments_per_unit_mass;
    CHECK(inertia.x > 0.0f && inertia.y > 0.0f && inertia.z > 0.0f);
    DeckGeometrySettings settings = DeckGeometrySettings::STOCK;
    settings.back_end_size *= 1.5f;
    const DeckGeometry extended = DeckGeometry::new_(settings);
    MassMoments extended_moments = extended.mass_moments();
    const auto extended_properties = extended_moments.principal_properties();
    CHECK(extended_properties.volume > properties.volume);
    CHECK_NE(extended_moments, moments);
    for (std::size_t index = 0; index < 5; ++index) {
        CHECK(stock.children[index] == extended.children[index]);
    }
    for (std::size_t index = 5; index < 15; ++index) {
        CHECK(stock.children[index] != extended.children[index]);
    }
}

S3_TEST(authored_count_controls_both_end_fans) {
    DeckGeometrySettings settings = DeckGeometrySettings::STOCK;
    settings.end_capsule_count = 3;
    CHECK_EQ(DeckGeometry::new_(settings).children.size(), std::size_t(11));
    settings.end_capsule_count = 0;
    CHECK_EQ(DeckGeometry::new_(settings).children.size(), std::size_t(5));
}
}  // namespace
