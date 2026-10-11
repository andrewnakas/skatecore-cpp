// Port of skate-core/src/physics/world_contact/point_feature.rs.
#include "s3/core/physics/world_contact/point_feature.hpp"

namespace s3::physics::world_contact::point_feature {

namespace {
using V = std::array<float, 4>;
V load(const std::uint32_t* words, std::size_t offset) {
    return V{rs::from_bits(words[offset]), rs::from_bits(words[offset + 1]), rs::from_bits(words[offset + 2]),
             rs::from_bits(words[offset + 3])};
}
float dot3(V a, V b) { return arithmetic::dot({a[0], a[1], a[2]}, {b[0], b[1], b[2]}); }
}  // namespace

std::uint32_t closest_feature_segment(const std::uint32_t* segment, std::uint32_t* point) {
    const V origin = load(segment, 0);
    const V direction = load(segment, 4);
    const V p = load(point, 0);
    const float along = dot3(V{p[0] - origin[0], p[1] - origin[1], p[2] - origin[2], p[3] - origin[3]}, direction);
    if (0.0f > along) {
        for (std::size_t i = 0; i < 4; ++i) point[i] = segment[i];
        return 1;
    }
    const V length = load(segment, 12);
    bool after = true;
    for (float v : length) after = after && along > v;
    for (std::size_t i = 0; i < 4; ++i) {
        point[i] = rs::to_bits(rs::mul_add(direction[i], after ? length[i] : along, origin[i]));
    }
    return after ? 3 : 2;
}

std::uint32_t intersect_point_face(FeaturePrism& output, MaximumFeature& face, const MaximumFeature& point,
                                   std::array<std::uint32_t, 4> normal_bits, bool face_is_a) {
    output[132] = 1;
    const V p = load(point.data(), 136);
    const V normal = rs::from_bits(normal_bits);
    const V origin = load(face.data(), 4);
    const float distance =
        dot3(normal, V{origin[0] - p[0], origin[1] - p[1], origin[2] - p[2], origin[3] - p[3]});
    V projected;
    for (std::size_t i = 0; i < 4; ++i) projected[i] = rs::mul_add(normal[i], distance, p[i]);
    if (static_cast<std::int32_t>(face[140]) > 0) {
        const std::size_t count = face[140];
        if (!(count <= 8)) S3_PANIC("native maximum-feature storage exceeded");
        std::size_t selected = 0;
        float worst = 0.0f;
        for (std::size_t edge = 0; edge < count; ++edge) {
            const V start = load(face.data(), 4 + edge * 16);
            const V plane = load(face.data(), 12 + edge * 16);
            const float value = dot3(plane, V{projected[0] - start[0], projected[1] - start[1],
                                              projected[2] - start[2], projected[3] - start[3]});
            if (edge == 0 || value > worst) {
                worst = value;
                selected = edge;
            }
        }
        if (worst > 0.0f) {
            const V plane = load(face.data(), 12 + selected * 16);
            std::array<std::uint32_t, 4> projected_bits;
            for (std::size_t i = 0; i < 4; ++i) projected_bits[i] = rs::to_bits(projected[i] - plane[i] * worst);
            const std::uint32_t region = closest_feature_segment(face.data() + 4 + selected * 16, projected_bits.data());
            face[0] = face[0] + (static_cast<std::uint32_t>(selected) * 2 + region);
            projected = rs::from_bits(projected_bits);
        }
    }
    const std::size_t face_offset = face_is_a ? 0 : 64;
    const std::size_t point_offset = face_is_a ? 64 : 0;
    for (std::size_t i = 0; i < 4; ++i) output[face_offset + i] = rs::to_bits(projected[i]);
    for (std::size_t i = 0; i < 4; ++i) output[point_offset + i] = point[136 + i];
    return 1;
}

}  // namespace s3::physics::world_contact::point_feature
