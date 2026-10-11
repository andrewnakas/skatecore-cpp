// Port of skate-core/src/physics/world_contact/segment_face.rs.
#include "s3/core/physics/world_contact/segment_face.hpp"

#include <algorithm>

#include "s3/core/physics/world_contact/face_clip.hpp"
#include "s3/core/physics/world_contact/point_feature.hpp"
#include "s3/core/physics/world_contact/prism_math.hpp"

namespace s3::physics::world_contact::segment_face {

using namespace prism_math;
using face_clip::clamp_point_to_feature;
using face_clip::clip_segment_to_feature;
using point_feature::closest_feature_segment;

namespace {
void store(FeaturePrism& output, std::size_t offset, V point) { store_bits(output.data(), offset, point); }
}  // namespace

std::uint32_t intersect_segment_face(FeaturePrism& output, MaximumFeature& face, MaximumFeature& segment,
                                     std::array<std::uint32_t, 4> normal_bits, bool face_is_a) {
    const std::size_t fo = face_is_a ? 0 : 64;
    const std::size_t so = face_is_a ? 64 : 0;
    std::array<std::uint32_t, 2> interval{0, 0};
    // The native caller explicitly zeroes both scratch vectors before clipping.
    std::array<std::uint32_t, 8> outside{};
    const std::uint32_t inside = clip_segment_to_feature(face, segment, normal_bits, interval, outside);
    const V normal = rs::from_bits(normal_bits);
    const V origin = load(segment, 4);
    const V direction = load(segment, 8);
    const V face_origin = load(face, 4);
    const float low = rs::from_bits(interval[0]);
    const float high = rs::from_bits(interval[1]);
    if (inside != 0) {
        output[132] = 2;
        const V face_normal = load(face, 132);
        const float distances[2] = {low, high};
        for (std::size_t i = 0; i < 2; ++i) {
            const V p = madd(direction, distances[i], origin);
            store(output, so + i * 4, p);
            const float t = dot(sub(face_origin, p), face_normal) / dot(normal, face_normal);
            store(output, fo + i * 4, madd(normal, t, p));
        }
        return 1;
    }
    const float span = high - low;
    if (span > rs::from_bits(0x3a83126fu)) {
        output[132] = 2;
        const V outside_origin = load(outside, 0);
        const V outside_plane = load(outside, 4);
        V projected = madd(normal, dot(normal, sub(face_origin, origin)), origin);
        projected = madd(outside_plane, dot(outside_plane, sub(outside_origin, projected)), projected);
        const V first_v = madd(direction, low, projected);
        const V second_v = madd(direction, span, first_v);
        std::array<std::uint32_t, 4> first = bits(first_v);
        std::array<std::uint32_t, 4> second = bits(second_v);
        const std::uint32_t first_code = clamp_point_to_feature(face, normal_bits, first);
        const std::uint32_t second_code = clamp_point_to_feature(face, normal_bits, second);
        face[0] = face[0] + (std::max(first_code, second_code) & ~1u);
        for (std::size_t i = 0; i < 4; ++i) output[fo + i] = first[i];
        for (std::size_t i = 0; i < 4; ++i) output[fo + 4 + i] = second[i];
        closest_feature_segment(segment.data() + 4, first.data());
        closest_feature_segment(segment.data() + 4, second.data());
        for (std::size_t i = 0; i < 4; ++i) output[so + i] = first[i];
        for (std::size_t i = 0; i < 4; ++i) output[so + 4 + i] = second[i];
        const V delta = sub(load(output, fo), rs::from_bits(first));
        const float squared = dot(delta, delta);
        const float length = squared == 0.0f ? 0.0f : squared * inverse_length(squared);
        if (length > rs::f32::MIN_POSITIVE) {
            output[133] = 1;
            store(output, 128, normalized(cross(direction, cross(delta, direction))));
        }
    } else {
        output[132] = 1;
        const V p0 = madd(direction, low, origin);
        std::array<std::uint32_t, 4> p = bits(madd(normal, dot(normal, sub(face_origin, p0)), p0));
        const std::uint32_t code = clamp_point_to_feature(face, normal_bits, p);
        face[0] = face[0] + code;
        for (std::size_t i = 0; i < 4; ++i) output[fo + i] = p[i];
        const std::uint32_t region = closest_feature_segment(segment.data() + 4, p.data());
        segment[0] = segment[0] - region + 2u;
        for (std::size_t i = 0; i < 4; ++i) output[so + i] = p[i];
    }
    return 1;
}

}  // namespace s3::physics::world_contact::segment_face
