// Port of skate-core/src/physics/world_contact/segment_prism.rs.
#include "s3/core/physics/world_contact/segment_prism.hpp"

#include "s3/core/physics/world_contact/point_feature.hpp"
#include "s3/core/physics/world_contact/prism_math.hpp"

namespace s3::physics::world_contact::segment_prism {

using namespace prism_math;
using point_feature::closest_feature_segment;

namespace {
void store(FeaturePrism& output, std::size_t offset, V point) { store_bits(output.data(), offset, point); }

V endpoint(MaximumFeature& feature, bool start) {
    feature[0] = start ? feature[0] - 1u : feature[0] + 1u;
    const V origin = load(feature, 4);
    if (start) {
        return origin;
    }
    const V direction = load(feature, 8);
    const V length = load(feature, 16);
    V out;
    for (std::size_t i = 0; i < 4; ++i) out[i] = rs::mul_add(direction[i], length[i], origin[i]);
    return out;
}
}  // namespace

std::uint32_t intersect_feature_segments(FeaturePrism& output, MaximumFeature& a, MaximumFeature& b,
                                         std::array<std::uint32_t, 4> normal_bits, bool a_is_first) {
    const std::size_t ao = a_is_first ? 0 : 64;
    const std::size_t bo = a_is_first ? 64 : 0;
    const V ad = load(a, 8);
    const V bd = load(b, 8);
    const V start_a = load(a, 4);
    const V start_b = load(b, 4);
    const V perpendicular = cross(ad, bd);
    if (dot(perpendicular, perpendicular) > rs::from_bits(0x34000000u)) {
        const V plane = cross(normalized(perpendicular), bd);
        float along = dot(sub(start_b, start_a), plane) / dot(ad, plane);
        if (along < 0.0f) {
            along = 0.0f;
        }
        const V length = load(a, 16);
        bool all = true;
        for (float v : length) all = all && along > v;
        if (all) {
            along = length[0];
        }
        output[132] = 1;
        std::array<std::uint32_t, 4> point = bits(madd(ad, along, start_a));
        std::uint32_t region = closest_feature_segment(b.data() + 4, point.data());
        b[0] = b[0] - region + 2u;
        for (std::size_t i = 0; i < 4; ++i) output[bo + i] = point[i];
        region = closest_feature_segment(a.data() + 4, point.data());
        a[0] = a[0] - region + 2u;
        for (std::size_t i = 0; i < 4; ++i) output[ao + i] = point[i];
        return 1;
    }
    const V normal = rs::from_bits(normal_bits);
    const V axis = rs::abs(dot(normal, ad)) < rs::abs(dot(normal, bd)) ? ad : bd;
    const V al = load(a, 16);
    const V bl = load(b, 16);
    V end_a, end_b;
    for (std::size_t i = 0; i < 4; ++i) end_a[i] = rs::mul_add(ad[i], al[i], start_a[i]);
    for (std::size_t i = 0; i < 4; ++i) end_b[i] = rs::mul_add(bd[i], bl[i], start_b[i]);
    const float a0 = dot(axis, start_a);
    const float b0 = dot(axis, start_b);
    const float a1 = dot(axis, end_a);
    const float b1 = dot(axis, end_b);
    // Native compare/select ordering also specifies unordered/equal operands.
    const float amin = a1 > a0 ? a0 : a1;
    const float amax = a1 > a0 ? a1 : a0;
    const float bmin = b1 > b0 ? b0 : b1;
    const float bmax = b1 > b0 ? b1 : b0;
    const float low = amin > bmin ? amin : bmin;
    const float high = bmax > amax ? amax : bmax;
    if (high > low) {
        const V ap = normalized(cross(cross(normal, ad), ad));
        const V bp = normalized(cross(cross(normal, bd), bd));
        const float alongs[2] = {low, high};
        for (std::size_t index = 0; index < 2; ++index) {
            const V anchor = scale(axis, alongs[index]);
            struct Side {
                std::size_t offset;
                V origin, direction, plane;
            };
            const Side sides[2] = {{ao, start_a, ad, ap}, {bo, start_b, bd, bp}};
            for (const Side& s : sides) {
                const float inverse = reciprocal(dot(normal, s.plane));
                const float distance = inverse * dot(sub(s.origin, anchor), s.plane);
                V projected = madd(normal, distance, anchor);
                projected = madd(s.direction, dot(sub(projected, s.origin), s.direction), s.origin);
                store(output, s.offset + index * 4, projected);
            }
        }
        output[132] = 2;
    } else {
        output[132] = 1;
        bool a_start, b_start;
        if (bmin > amin) {
            a_start = a0 > a1;
            b_start = b0 < b1;
        } else {
            a_start = a0 < a1;
            b_start = b0 > b1;
        }
        store(output, ao, endpoint(a, a_start));
        store(output, bo, endpoint(b, b_start));
    }
    return 1;
}

}  // namespace s3::physics::world_contact::segment_prism
