// Port of skate-core/src/physics/world_contact/dispatch.rs.
#include "s3/core/physics/world_contact/dispatch.hpp"

#include "s3/core/physics/world_contact/point_feature.hpp"
#include "s3/core/physics/world_contact/segment_face.hpp"
#include "s3/core/physics/world_contact/segment_prism.hpp"

namespace s3::physics::world_contact::dispatch {

using point_feature::closest_feature_segment;
using point_feature::intersect_point_face;
using segment_face::intersect_segment_face;
using segment_prism::intersect_feature_segments;

namespace {
std::uint32_t segment_point(FeaturePrism& output, MaximumFeature& segment, const MaximumFeature& point,
                            bool segment_is_a) {
    output[132] = 1;
    for (std::size_t i = 0; i < 4; ++i) output[i] = point[136 + i];
    for (std::size_t i = 0; i < 4; ++i) output[64 + i] = point[136 + i];
    const std::size_t offset = segment_is_a ? 0 : 64;
    const std::uint32_t region = closest_feature_segment(segment.data() + 4, output.data() + offset);
    segment[0] = segment[0] - region + 2u;
    return 1;
}
}  // namespace

std::uint32_t find_feature_intersection_prism(FeaturePrism& output, MaximumFeature& a, MaximumFeature& b,
                                              std::array<std::uint32_t, 4> normal) {
    const std::uint32_t a_edges = a[140], b_edges = b[140];
    if (!(a_edges <= 8 && b_edges <= 8)) S3_PANIC("maximum-feature storage exceeded");
    if (a_edges == 0 && b_edges == 0) {
        output[132] = 1;
        for (std::size_t i = 0; i < 4; ++i) output[i] = a[136 + i];
        for (std::size_t i = 0; i < 4; ++i) output[64 + i] = b[136 + i];
        return 1;
    } else if (a_edges == 1 && b_edges == 0) {
        return segment_point(output, a, b, true);
    } else if (a_edges == 0 && b_edges == 1) {
        return segment_point(output, b, a, false);
    } else if (a_edges == 0) {
        return intersect_point_face(output, b, a, normal, false);
    } else if (b_edges == 0) {
        return intersect_point_face(output, a, b, normal, true);
    } else if (a_edges == 1 && b_edges == 1) {
        return intersect_feature_segments(output, a, b, normal, true);
    } else if (a_edges == 1) {
        return intersect_segment_face(output, b, a, normal, false);
    } else if (b_edges == 1) {
        return intersect_segment_face(output, a, b, normal, true);
    } else if (a_edges == 4 && b_edges == 3) {
        if (face_specialized::quad_triangle(output, a, b, normal, true) != 0) {
            return 1;
        }
    } else if (a_edges == 3 && b_edges == 4) {
        if (face_specialized::quad_triangle(output, b, a, normal, false) != 0) {
            return 1;
        }
    } else if (a_edges == 4 && b_edges == 4) {
        if (face_specialized::quad_quad(output, a, b, normal) != 0) {
            return 1;
        }
    }
    return face_face::intersect(output, a, b);
}

}  // namespace s3::physics::world_contact::dispatch
