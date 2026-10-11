// Port of skate-core/src/physics/world_contact/face_specialized.rs.
#include "s3/core/physics/world_contact/face_specialized.hpp"

#include "s3/core/physics/world_contact/prism_math.hpp"

namespace s3::physics::world_contact::dispatch::face_specialized {

using namespace prism_math;

namespace {
V vertex(const MaximumFeature& feature, std::size_t edge) { return load(feature, 4 + edge * 16); }

V project_to_face(const MaximumFeature& face, V point) {
    const V normal = load(face, 132);
    return madd(normal, dot(normal, sub(vertex(face, 0), point)), point);
}

void append(FeaturePrism& output, std::size_t& count, const MaximumFeature& a, const MaximumFeature& b, V point,
            bool a_is_first) {
    if (!(count < 16)) S3_PANIC("feature prism point capacity exceeded");
    const std::size_t ao = a_is_first ? 0 : 64;
    const std::size_t bo = a_is_first ? 64 : 0;
    const std::size_t offset = count * 4;
    store_bits(output.data(), ao + offset, project_to_face(a, point));
    store_bits(output.data(), bo + offset, project_to_face(b, point));
    count += 1;
}

// The contained-vertex passes use lane-wise multiply/FMA operations, with a
// rounded Y product followed by X and Z FMAs, instead of the VMX three-lane dot.
float containment_dot(V delta, V normal) {
    return rs::mul_add(delta[2], normal[2], rs::mul_add(delta[0], normal[0], delta[1] * normal[1]));
}

std::uint32_t intersect(FeaturePrism& output, const MaximumFeature& a, const MaximumFeature& b,
                        std::array<std::uint32_t, 4> normal_bits, bool a_is_first) {
    std::size_t count = 0;
    const std::size_t b_count = b[140];
    // Full native edge-pair validity masks, including endpoint equality.
    // Only the first lane of each mask controls publication, so broadcast
    // feature lengths are read from their first lane here as in the source.
    for (std::size_t edge_a = 0; edge_a < 4; ++edge_a) {
        const V a_origin = vertex(a, edge_a);
        const V a_direction = load(a, 8 + edge_a * 16);
        const V a_plane = load(a, 12 + edge_a * 16);
        const float a_length = rs::from_bits(a[16 + edge_a * 16]);
        for (std::size_t edge_b = 0; edge_b < b_count; ++edge_b) {
            const V b_origin = vertex(b, edge_b);
            const V b_direction = load(b, 8 + edge_b * 16);
            const float b_length = rs::from_bits(b[16 + edge_b * 16]);
            const float denominator = dot(b_direction, a_plane);
            const float offset = dot(sub(a_origin, b_origin), a_plane);
            const V point = madd(b_direction, reciprocal(denominator) * offset, b_origin);
            const float along_a = dot(sub(point, a_origin), a_direction);
            const bool valid = rs::abs(denominator) > rs::from_bits(0x34000000u) &&
                               !(rs::abs(offset) > rs::abs(b_length * denominator)) &&
                               !(0.0f > offset * denominator) && !(along_a > a_length) && !(0.0f > along_a);
            if (valid) {
                append(output, count, a, b, point, a_is_first);
            }
        }
    }
    const V normal = rs::from_bits(normal_bits);
    struct Pass {
        const MaximumFeature* source;
        const MaximumFeature* target;
        std::size_t source_count, target_count;
    };
    const Pass passes[2] = {{&a, &b, 4, b_count}, {&b, &a, b_count, 4}};
    for (const Pass& pass : passes) {
        for (std::size_t corner = 0; corner < pass.source_count; ++corner) {
            const V origin = vertex(*pass.source, corner);
            const float distance = containment_dot(sub(vertex(*pass.target, 0), origin), normal);
            // The source transposes XYZ back into points with a literal W=1.
            V point = madd(normal, distance, origin);
            point[3] = 1.0f;
            bool inside = true;
            for (std::size_t edge = 0; edge < pass.target_count; ++edge) {
                const float d = containment_dot(sub(point, vertex(*pass.target, edge)),
                                                load(*pass.target, 12 + edge * 16));
                if (!(0.0f > d)) {
                    inside = false;
                    break;
                }
            }
            if (inside) {
                append(output, count, a, b, point, a_is_first);
            }
        }
    }
    output[132] = static_cast<std::uint32_t>(count);
    return std::uint32_t(count != 0);
}
}  // namespace

std::uint32_t quad_triangle(FeaturePrism& output, const MaximumFeature& quad, const MaximumFeature& triangle,
                            std::array<std::uint32_t, 4> normal, bool quad_is_a) {
    return intersect(output, quad, triangle, normal, quad_is_a);
}

std::uint32_t quad_quad(FeaturePrism& output, const MaximumFeature& a, const MaximumFeature& b,
                        std::array<std::uint32_t, 4> normal) {
    return intersect(output, a, b, normal, true);
}

}  // namespace s3::physics::world_contact::dispatch::face_specialized
