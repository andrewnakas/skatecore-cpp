// Port of skate-core/src/physics/world_contact/face_clip.rs.
#include "s3/core/physics/world_contact/face_clip.hpp"

#include <algorithm>

#include "s3/core/physics/world_contact/point_feature.hpp"
#include "s3/core/physics/world_contact/prism_math.hpp"

namespace s3::physics::world_contact::face_clip {

using namespace prism_math;
using point_feature::closest_feature_segment;

std::uint32_t clamp_point_to_feature(const MaximumFeature& face, std::array<std::uint32_t, 4> normal_bits,
                                     std::array<std::uint32_t, 4>& point) {
    const V normal = rs::from_bits(normal_bits);
    const V p = rs::from_bits(point);
    const std::size_t count = static_cast<std::size_t>(std::max(static_cast<std::int32_t>(face[140]), 0));
    if (!(count <= 8)) S3_PANIC("native maximum-feature storage exceeded");
    std::size_t selected = 0;
    float minimum = 0.0f;
    V selected_plane{0.0f, 0.0f, 0.0f, 0.0f};
    for (std::size_t edge = 0; edge < count; ++edge) {
        V plane = cross(normal, load(face, 8 + edge * 16));
        if (dot(plane, plane) > rs::from_bits(0x34000000u)) {
            plane = normalized(plane);
        }
        const float value = dot(plane, sub(p, load(face, 4 + edge * 16)));
        if (edge == 0 || minimum > value) {
            minimum = value;
            selected = edge;
            selected_plane = plane;
        }
    }
    if (0.0f > minimum) {
        point = bits(sub(p, scale(selected_plane, minimum)));
        return closest_feature_segment(face.data() + 4 + selected * 16, point.data()) +
               static_cast<std::uint32_t>(selected) * 2;
    }
    return 0;
}

std::uint32_t clip_segment_to_feature(const MaximumFeature& face, const MaximumFeature& segment,
                                      std::array<std::uint32_t, 4> normal_bits, std::array<std::uint32_t, 2>& interval,
                                      std::array<std::uint32_t, 8>& outside) {
    interval[0] = 0;
    // lfs/stfs quiet an input signalling NaN in the scalar length field.
    interval[1] = rs::is_nan(rs::from_bits(segment[16])) ? (segment[16] | 0x00400000u) : segment[16];
    const V normal = rs::from_bits(normal_bits);
    const std::size_t count = static_cast<std::size_t>(std::max(static_cast<std::int32_t>(face[140]), 0));
    if (!(count <= 8)) S3_PANIC("native maximum-feature storage exceeded");
    bool rejected = false;
    for (std::size_t edge = 0; edge < count; ++edge) {
        const V raw_plane = cross(normal, load(face, 8 + edge * 16));
        const float squared = dot(raw_plane, raw_plane);
        const float length = squared == 0.0f ? 0.0f : squared * prism_math::inverse_length(squared);
        if (length < rs::from_bits(0x3a83126fu)) {
            continue;
        }
        const V plane = normalized(raw_plane);
        const V origin = load(face, 4 + edge * 16);
        const float denominator = dot(load(segment, 8), plane);
        const float numerator = dot(sub(origin, load(segment, 4)), plane);
        if (rs::abs(denominator) < rs::from_bits(0x3d4ccccdu)) {
            if (numerator > 0.0f) {
                rejected = true;
                for (std::size_t i = 0; i < 4; ++i) outside[i] = face[4 + edge * 16 + i];
                const auto plane_bits = bits(plane);
                for (std::size_t i = 0; i < 4; ++i) outside[4 + i] = plane_bits[i];
            }
        } else {
            const float position = numerator / denominator;
            if (denominator > 0.0f) {
                if (position > rs::from_bits(interval[0])) {
                    interval[0] = rs::to_bits(position);
                }
                if (rs::from_bits(interval[0]) > rs::from_bits(interval[1])) {
                    interval[0] = interval[1];
                    return 0;
                }
            } else {
                if (position < rs::from_bits(interval[1])) {
                    interval[1] = rs::to_bits(position);
                }
                if (rs::from_bits(interval[1]) < rs::from_bits(interval[0])) {
                    interval[1] = interval[0];
                    return 0;
                }
            }
        }
    }
    return std::uint32_t(!rejected);
}

}  // namespace s3::physics::world_contact::face_clip
