// Port of skate-core/src/physics/world_contact/face_face.rs.
#include "s3/core/physics/world_contact/face_face.hpp"

#include "s3/core/physics/world_contact/face_edge.hpp"
#include "s3/core/physics/world_contact/prism_math.hpp"

namespace s3::physics::world_contact::dispatch::face_face {

using namespace prism_math;
using face_edge::intersect_feature_corner_edge;

namespace {
constexpr std::uint32_t OUTSIDE_A = 0x40000000u;
constexpr std::uint32_t OUTSIDE_B = 0x20000000u;

V vertex(const MaximumFeature& feature, std::size_t index) { return load(feature, 4 + index * 16); }

float plane_distance(const MaximumFeature& feature, std::size_t edge, V point) {
    return dot(load(feature, 12 + edge * 16), sub(point, vertex(feature, edge)));
}

V project_to_face(const MaximumFeature& face, V point) {
    const V normal = load(face, 132);
    return madd(normal, dot(normal, sub(vertex(face, 0), point)), point);
}

void append(FeaturePrism& output, V a, V b) {
    const std::size_t offset = static_cast<std::size_t>(output[132]) * 4;
    if (!(offset < 64)) S3_PANIC("feature prism point capacity exceeded");
    store_bits(output.data(), offset, a);
    store_bits(output.data(), 64 + offset, b);
    output[132] += 1;
}
}  // namespace

std::uint32_t intersect(FeaturePrism& output, MaximumFeature& a, MaximumFeature& b) {
    const std::size_t a_count = a[140], b_count = b[140];
    std::array<std::uint32_t, 8> masks{};
    std::size_t nearest_corner = 0;
    std::size_t nearest_edge = 0;
    bool edge_is_a = false;
    float largest_minimum = -rs::f32::MAX;
    output[132] = 0;

    for (std::size_t edge = 0; edge < a_count; ++edge) {
        std::size_t previous = b_count - 1;
        float previous_distance = plane_distance(a, edge, vertex(b, previous));
        float minimum = previous_distance;
        std::size_t minimum_corner = previous;
        for (std::size_t corner = 0; corner < b_count; ++corner) {
            const float distance = plane_distance(a, edge, vertex(b, corner));
            if (distance < minimum) {
                minimum = distance;
                minimum_corner = corner;
            }
            if (distance > 0.0f) {
                masks[corner] |= OUTSIDE_B;
            }
            if (!(distance * previous_distance > 0.0f)) {
                masks[previous] |= 1u << edge;
            }
            previous = corner;
            previous_distance = distance;
        }
        if (minimum > largest_minimum) {
            largest_minimum = minimum;
            nearest_corner = minimum_corner;
            nearest_edge = edge;
            edge_is_a = true;
        }
    }

    for (std::size_t corner = 0; corner < b_count; ++corner) {
        if ((masks[corner] & OUTSIDE_B) == 0) {
            const V point = vertex(b, corner);
            append(output, project_to_face(a, point), point);
        }
    }
    // Containment is an early return; it must not duplicate all of A's vertices
    // when coincident faces have already contributed every B vertex.
    if (static_cast<std::size_t>(output[132]) == b_count) {
        return 1;
    }

    for (std::size_t edge = 0; edge < b_count; ++edge) {
        std::size_t previous = a_count - 1;
        float previous_distance = plane_distance(b, edge, vertex(a, previous));
        float minimum = previous_distance;
        std::size_t minimum_corner = previous;
        for (std::size_t corner = 0; corner < a_count; ++corner) {
            const float distance = plane_distance(b, edge, vertex(a, corner));
            if (distance < minimum) {
                minimum = distance;
                minimum_corner = corner;
            }
            if (distance > 0.0f) {
                masks[corner] |= OUTSIDE_A;
            }
            const float difference = previous_distance - distance;
            if (!(distance * previous_distance > 0.0f) && (masks[edge] & (1u << previous)) != 0 &&
                rs::abs(difference) > rs::f32::MIN_POSITIVE) {
                const float inverse_difference = 1.0f / difference;
                const V origin = vertex(a, previous);
                const V direction = load(a, 8 + previous * 16);
                const V length = load(a, 16 + previous * 16);
                V point;
                for (std::size_t lane = 0; lane < 4; ++lane) {
                    const float numerator = length[lane] * previous_distance;
                    const float along = inverse_difference * numerator;
                    point[lane] = rs::mul_add(direction[lane], along, origin[lane]);
                }
                append(output, point, project_to_face(b, point));
            }
            previous = corner;
            previous_distance = distance;
        }
        if (minimum > largest_minimum) {
            largest_minimum = minimum;
            nearest_corner = minimum_corner;
            nearest_edge = edge;
            edge_is_a = false;
        }
    }
    for (std::size_t corner = 0; corner < a_count; ++corner) {
        if ((masks[corner] & OUTSIDE_A) == 0) {
            const V point = vertex(a, corner);
            append(output, point, project_to_face(b, point));
        }
    }
    if (output[132] == 0) {
        output[132] = edge_is_a ? intersect_feature_corner_edge(output, b, a, nearest_corner, nearest_edge, false)
                                : intersect_feature_corner_edge(output, a, b, nearest_corner, nearest_edge, true);
        const V delta = sub(load(output, 64), load(output, 0));
        const float squared = dot(delta, delta);
        const float length = squared == 0.0f ? 0.0f : squared * inverse_length(squared);
        if (length > rs::f32::MIN_POSITIVE) {
            store_bits(output.data(), 128, scale(delta, 1.0f / length));
            output[133] = 1;
        }
    }
    return 1;
}

}  // namespace s3::physics::world_contact::dispatch::face_face
