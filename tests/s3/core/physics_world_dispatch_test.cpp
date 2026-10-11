// Port of skate-core/src/physics/tests/world_dispatch.rs.
#include <vector>

#include "harness.hpp"
#include "s3/core/physics/world_contact.hpp"

namespace {
using s3::physics::world_contact::FeaturePrism;
using s3::physics::world_contact::find_feature_intersection_prism;
using s3::physics::world_contact::MaximumFeature;
using s3::rs::from_bits;
using s3::rs::to_bits;
using U4 = std::array<std::uint32_t, 4>;

const U4 NORMAL{0, to_bits(1.0f), 0, 0};
constexpr std::uint32_t SENTINEL = 0xdeadbeefu;

U4 vector(std::array<float, 4> value) { return s3::rs::to_bits(value); }
void copy4(std::uint32_t* dst, const U4& src) {
    for (std::size_t i = 0; i < 4; ++i) dst[i] = src[i];
}
MaximumFeature point(float x, float y, float z) {
    MaximumFeature feature{};
    feature[0] = 100;
    copy4(feature.data() + 136, vector({x, y, z, 1.0f}));
    return feature;
}
MaximumFeature segment(std::array<float, 3> start, std::array<float, 3> end) {
    MaximumFeature feature{};
    feature[0] = 100;
    feature[140] = 1;
    std::array<float, 3> delta;
    for (std::size_t i = 0; i < 3; ++i) delta[i] = end[i] - start[i];
    float sum = -0.0f;
    for (float v : delta) sum += v * v;
    const float length = s3::rs::sqrt(sum);
    copy4(feature.data() + 4, vector({start[0], start[1], start[2], 1.0f}));
    copy4(feature.data() + 8, vector({delta[0] / length, delta[1] / length, delta[2] / length, 0.0f}));
    for (std::size_t i = 16; i < 20; ++i) feature[i] = to_bits(length);
    return feature;
}
MaximumFeature face(const std::vector<std::array<float, 2>>& vertices, float height) {
    MaximumFeature feature{};
    feature[0] = 100;
    feature[140] = static_cast<std::uint32_t>(vertices.size());
    copy4(feature.data() + 132, NORMAL);
    for (std::size_t edge = 0; edge < vertices.size(); ++edge) {
        const auto start = vertices[edge];
        const auto end = vertices[(edge + 1) % vertices.size()];
        const MaximumFeature line = segment({start[0], height, start[1]}, {end[0], height, end[1]});
        const std::size_t offset = 4 + edge * 16;
        for (std::size_t i = 0; i < 16; ++i) feature[offset + i] = line[4 + i];
        const float dx = from_bits(line[8]);
        const float dz = from_bits(line[10]);
        copy4(feature.data() + offset + 8, vector({dz, 0.0f, -dx, 0.0f}));
    }
    return feature;
}
MaximumFeature square(float x, float z, float height) {
    return face({{x - 1.0f, z - 1.0f}, {x + 1.0f, z - 1.0f}, {x + 1.0f, z + 1.0f}, {x - 1.0f, z + 1.0f}}, height);
}
FeaturePrism query(MaximumFeature& a, MaximumFeature& b) {
    FeaturePrism result;
    result.fill(SENTINEL);
    copy4(result.data() + 128, NORMAL);
    result[133] = 0;
    CHECK_EQ(find_feature_intersection_prism(result, a, b, NORMAL), 1u);
    return result;
}
void assert_point(const FeaturePrism& output, std::size_t side, std::size_t index, std::array<float, 3> expected) {
    for (std::size_t axis = 0; axis < 3; ++axis) {
        const float actual = from_bits(output[side * 64 + index * 4 + axis]);
        CHECK_MSG(s3::rs::abs(actual - expected[axis]) < 2.0e-5f, "side=" << side << " point=" << index << " axis="
                                                                        << axis << ": " << actual
                                                                        << " != " << expected[axis]);
    }
}
void assert_untouched_tail(const FeaturePrism& output) {
    const std::size_t count = output[132];
    for (std::size_t i = count * 4; i < 64; ++i) CHECK(output[i] == SENTINEL);
    for (std::size_t i = 64 + count * 4; i < 128; ++i) CHECK(output[i] == SENTINEL);
    CHECK_EQ(output[134], SENTINEL);
    CHECK_EQ(output[135], SENTINEL);
}
bool eq4(const std::uint32_t* a, const std::uint32_t* b) {
    for (std::size_t i = 0; i < 4; ++i)
        if (a[i] != b[i]) return false;
    return true;
}
std::size_t uz(bool b) { return b ? 1 : 0; }

S3_TEST(point_point_copies_both_points_and_preserves_prism_state) {
    MaximumFeature a = point(2.0f, 3.0f, 4.0f);
    MaximumFeature b = point(-3.0f, 8.0f, 6.0f);
    const FeaturePrism output = query(a, b);
    CHECK_EQ(output[132], 1u);
    CHECK(eq4(output.data(), a.data() + 136));
    CHECK(eq4(output.data() + 64, b.data() + 136));
    CHECK(eq4(output.data() + 128, NORMAL.data()));
    CHECK_EQ(output[133], 0u);
    CHECK_EQ(a[0], 100u);
    CHECK_EQ(b[0], 100u);
    assert_untouched_tail(output);
}

S3_TEST(point_segment_selects_before_interior_and_after_tags_in_both_orders) {
    struct Case {
        float x, projected;
        std::uint32_t tag;
    };
    const Case cases[] = {{-1.0f, 0.0f, 101}, {0.0f, 0.0f, 100}, {1.0f, 1.0f, 100}, {2.0f, 2.0f, 100}, {3.0f, 2.0f, 99}};
    for (const Case& c : cases) {
        for (bool reverse : {false, true}) {
            MaximumFeature line = segment({0.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f});
            MaximumFeature p = point(c.x, 2.0f, 0.0f);
            const FeaturePrism output = reverse ? query(p, line) : query(line, p);
            CHECK_EQ(output[132], 1u);
            assert_point(output, uz(reverse), 0, {c.projected, 0.0f, 0.0f});
            assert_point(output, uz(!reverse), 0, {c.x, 2.0f, 0.0f});
            CHECK_EQ(line[0], c.tag);
            CHECK_EQ(p[0], 100u);
            assert_untouched_tail(output);
        }
    }
}

S3_TEST(point_face_projects_and_clamps_to_boundary_in_both_orders) {
    struct Case {
        float z, projected;
        std::uint32_t tag;
    };
    const Case cases[] = {{0.0f, 0.0f, 100}, {-2.0f, -1.0f, 102}};
    for (const Case& c : cases) {
        for (bool reverse : {false, true}) {
            MaximumFeature polygon = square(0.0f, 0.0f, 0.0f);
            MaximumFeature p = point(0.0f, 2.0f, c.z);
            const FeaturePrism output = reverse ? query(p, polygon) : query(polygon, p);
            CHECK_EQ(output[132], 1u);
            assert_point(output, uz(reverse), 0, {0.0f, 0.0f, c.projected});
            assert_point(output, uz(!reverse), 0, {0.0f, 2.0f, c.z});
            CHECK_EQ(polygon[0], c.tag);
            assert_untouched_tail(output);
        }
    }
}

S3_TEST(segment_segment_covers_crossing_overlap_and_disjoint_endpoints) {
    MaximumFeature a = segment({-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f});
    MaximumFeature b = segment({0.0f, 1.0f, -1.0f}, {0.0f, 1.0f, 1.0f});
    const FeaturePrism crossing = query(a, b);
    CHECK_EQ(crossing[132], 1u);
    assert_point(crossing, 0, 0, {0.0f, 0.0f, 0.0f});
    assert_point(crossing, 1, 0, {0.0f, 1.0f, 0.0f});
    b = segment({0.0f, 1.0f, 0.0f}, {2.0f, 1.0f, 0.0f});
    const FeaturePrism overlap = query(a, b);
    CHECK_EQ(overlap[132], 2u);
    for (std::size_t side = 0; side < 2; ++side) {
        assert_point(overlap, side, 0, {0.0f, static_cast<float>(side), 0.0f});
        assert_point(overlap, side, 1, {1.0f, static_cast<float>(side), 0.0f});
    }
    b = segment({3.0f, 1.0f, 0.0f}, {4.0f, 1.0f, 0.0f});
    const FeaturePrism disjoint = query(a, b);
    CHECK_EQ(disjoint[132], 1u);
    assert_point(disjoint, 0, 0, {1.0f, 0.0f, 0.0f});
    assert_point(disjoint, 1, 0, {3.0f, 1.0f, 0.0f});
}

S3_TEST(segment_face_keeps_clipped_two_point_manifold_in_both_orders) {
    for (bool reverse : {false, true}) {
        MaximumFeature polygon = square(0.0f, 0.0f, 0.0f);
        MaximumFeature line = segment({-2.0f, 1.0f, 0.0f}, {2.0f, 1.0f, 0.0f});
        // For this winding, -Y cross edge points into the clipping polygon.
        const U4 direction = vector({0.0f, -1.0f, 0.0f, 0.0f});
        FeaturePrism output;
        output.fill(SENTINEL);
        copy4(output.data() + 128, direction);
        output[133] = 0;
        MaximumFeature& a = reverse ? line : polygon;
        MaximumFeature& b = reverse ? polygon : line;
        CHECK_EQ(find_feature_intersection_prism(output, a, b, direction), 1u);
        CHECK_EQ(output[132], 2u);
        const float xs[2] = {-1.0f, 1.0f};
        for (std::size_t index = 0; index < 2; ++index) {
            assert_point(output, uz(reverse), index, {xs[index], 0.0f, 0.0f});
            assert_point(output, uz(!reverse), index, {xs[index], 1.0f, 0.0f});
        }
        assert_untouched_tail(output);
    }
}

S3_TEST(quad_triangle_emits_edge_pairs_then_inside_vertex_with_stable_swapped_order) {
    const float expected[5][2] = {{1.0f, -0.5f}, {1.0f, 0.5f}, {0.5f, 1.0f}, {0.0f, 1.0f}, {0.0f, -0.5f}};
    for (bool reverse : {false, true}) {
        MaximumFeature quad = square(0.0f, 0.0f, 0.0f);
        MaximumFeature triangle = face({{0.0f, -0.5f}, {2.0f, -0.5f}, {0.0f, 1.5f}}, 1.0f);
        const FeaturePrism output = reverse ? query(triangle, quad) : query(quad, triangle);
        CHECK_EQ(output[132], 5u);
        for (std::size_t index = 0; index < 5; ++index) {
            const float x = expected[index][0], z = expected[index][1];
            assert_point(output, uz(reverse), index, {x, 0.0f, z});
            assert_point(output, uz(!reverse), index, {x, 1.0f, z});
        }
        CHECK_EQ(output[133], 0u);
        CHECK_EQ(quad[0], 100u);
        CHECK_EQ(triangle[0], 100u);
        assert_untouched_tail(output);
    }
}

S3_TEST(quad_quad_orders_crossings_before_both_containment_passes) {
    MaximumFeature a = square(0.0f, 0.0f, 0.0f);
    MaximumFeature b = square(1.0f, 0.5f, 1.0f);
    const FeaturePrism output = query(a, b);
    CHECK_EQ(output[132], 4u);
    const float expected[4][2] = {{1.0f, -0.5f}, {0.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, -0.5f}};
    for (std::size_t index = 0; index < 4; ++index) {
        assert_point(output, 0, index, {expected[index][0], 0.0f, expected[index][1]});
        assert_point(output, 1, index, {expected[index][0], 1.0f, expected[index][1]});
    }
    assert_untouched_tail(output);
}

S3_TEST(coincident_quad_boundaries_keep_native_duplicate_edge_intersections) {
    MaximumFeature a = square(0.0f, 0.0f, 0.0f);
    MaximumFeature b = square(0.0f, 0.0f, 1.0f);
    const FeaturePrism output = query(a, b);
    CHECK_EQ(output[132], 8u);
    for (std::size_t index = 0; index < 8; ++index) {
        CHECK_EQ(s3::rs::abs(from_bits(output[index * 4])), 1.0f);
        CHECK_EQ(s3::rs::abs(from_bits(output[index * 4 + 2])), 1.0f);
    }
    assert_untouched_tail(output);
}

S3_TEST(general_triangle_containment_returns_b_vertices_without_duplicates) {
    const std::vector<std::array<float, 2>> vertices = {{-1.0f, -1.0f}, {1.0f, -1.0f}, {0.0f, 1.0f}};
    MaximumFeature a = face(vertices, 0.0f);
    MaximumFeature b = face(vertices, 1.0f);
    const FeaturePrism output = query(a, b);
    CHECK_EQ(output[132], 3u);
    for (std::size_t index = 0; index < 3; ++index) {
        assert_point(output, 0, index, {vertices[index][0], 0.0f, vertices[index][1]});
        assert_point(output, 1, index, {vertices[index][0], 1.0f, vertices[index][1]});
    }
    assert_untouched_tail(output);
}

S3_TEST(general_triangle_crossings_form_the_six_corner_intersection) {
    MaximumFeature a = face({{-2.0f, -1.0f}, {2.0f, -1.0f}, {0.0f, 2.0f}}, 0.0f);
    MaximumFeature b = face({{-2.0f, 1.0f}, {0.0f, -2.0f}, {2.0f, 1.0f}}, 1.0f);
    const FeaturePrism output = query(a, b);
    CHECK_EQ(output[132], 6u);
    const float expected[6][2] = {
        {-4.0f / 3.0f, 0.0f}, {-2.0f / 3.0f, -1.0f}, {2.0f / 3.0f, -1.0f},
        {4.0f / 3.0f, 0.0f},  {2.0f / 3.0f, 1.0f},   {-2.0f / 3.0f, 1.0f},
    };
    for (const auto& e : expected) {
        bool any = false;
        for (std::size_t index = 0; index < 6; ++index) {
            any = any || (s3::rs::abs(from_bits(output[index * 4]) - e[0]) < 2.0e-5f &&
                          s3::rs::abs(from_bits(output[index * 4 + 2]) - e[1]) < 2.0e-5f);
        }
        CHECK(any);
    }
    for (std::size_t index = 0; index < 6; ++index) {
        const float x = from_bits(output[index * 4]);
        const float z = from_bits(output[index * 4 + 2]);
        assert_point(output, 1, index, {x, 1.0f, z});
    }
    assert_untouched_tail(output);
}

S3_TEST(disjoint_faces_fall_back_to_closest_features_and_correct_normal) {
    MaximumFeature a = face({{0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 1.0f}}, 0.0f);
    MaximumFeature b = face({{3.0f, 3.0f}, {4.0f, 3.0f}, {3.0f, 4.0f}}, 0.0f);
    const FeaturePrism output = query(a, b);
    CHECK_EQ(output[132], 1u);
    assert_point(output, 0, 0, {0.5f, 0.0f, 0.5f});
    assert_point(output, 1, 0, {3.0f, 0.0f, 3.0f});
    CHECK_EQ(a[0], 4u);
    CHECK_EQ(b[0], 1u);
    CHECK_EQ(output[133], 1u);
    CHECK(s3::rs::abs(from_bits(output[128]) - s3::rs::f32::consts::FRAC_1_SQRT_2) < 2.0e-5f);
    CHECK(s3::rs::abs(from_bits(output[130]) - s3::rs::f32::consts::FRAC_1_SQRT_2) < 2.0e-5f);
    assert_untouched_tail(output);
}

S3_TEST(specialized_quad_rejection_reaches_parallel_edge_fallback) {
    MaximumFeature a = square(0.0f, 0.0f, 0.0f);
    MaximumFeature b = square(3.0f, 0.0f, 0.0f);
    const FeaturePrism output = query(a, b);
    CHECK_EQ(output[132], 2u);
    const float zs[2] = {-1.0f, 1.0f};
    for (std::size_t index = 0; index < 2; ++index) {
        assert_point(output, 0, index, {1.0f, 0.0f, zs[index]});
        assert_point(output, 1, index, {2.0f, 0.0f, zs[index]});
    }
    CHECK_EQ(a[0], 4u);
    CHECK_EQ(b[0], 8u);
    CHECK_EQ(output[133], 1u);
    CHECK(eq4(output.data() + 128, vector({1.0f, 0.0f, 0.0f, 0.0f}).data()));
    assert_untouched_tail(output);
}

S3_TEST(specialized_containment_handles_each_face_as_the_enclosed_feature) {
    for (bool triangle_is_enclosed : {false, true}) {
        for (bool reverse : {false, true}) {
            MaximumFeature quad = square(0.0f, 0.0f, 0.0f);
            MaximumFeature triangle;
            std::vector<std::array<float, 2>> expected;
            if (triangle_is_enclosed) {
                triangle = face({{-0.5f, -0.5f}, {0.5f, -0.5f}, {0.0f, 0.5f}}, 1.0f);
                expected = {{-0.5f, -0.5f}, {0.5f, -0.5f}, {0.0f, 0.5f}};
            } else {
                triangle = face({{-5.0f, -3.0f}, {5.0f, -3.0f}, {0.0f, 5.0f}}, 1.0f);
                expected = {{-1.0f, -1.0f}, {1.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 1.0f}};
            }
            const FeaturePrism output = reverse ? query(triangle, quad) : query(quad, triangle);
            CHECK_EQ(static_cast<std::size_t>(output[132]), expected.size());
            for (std::size_t index = 0; index < expected.size(); ++index) {
                const float x = expected[index][0], z = expected[index][1];
                assert_point(output, uz(reverse), index, {x, 0.0f, z});
                assert_point(output, uz(!reverse), index, {x, 1.0f, z});
                CHECK_EQ(output[index * 4 + 3], to_bits(1.0f));
                CHECK_EQ(output[64 + index * 4 + 3], to_bits(1.0f));
            }
            CHECK_EQ(output[133], 0u);
            assert_untouched_tail(output);
        }
    }
}
}  // namespace
