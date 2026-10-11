// Port of skate-core/src/physics/world_contact/face_edge.rs.
#include "s3/core/physics/world_contact/face_edge.hpp"

#include "s3/core/physics/world_contact/prism_math.hpp"

namespace s3::physics::world_contact::face_edge {

using namespace prism_math;

namespace {
void store(FeaturePrism& output, std::size_t offset, V point) { store_bits(output.data(), offset, point); }

V end(const MaximumFeature& feature, std::size_t edge) {
    const V origin = load(feature, 4 + edge * 16);
    const V direction = load(feature, 8 + edge * 16);
    const V length = load(feature, 16 + edge * 16);
    V out;
    for (std::size_t i = 0; i < 4; ++i) out[i] = rs::mul_add(direction[i], length[i], origin[i]);
    return out;
}
}  // namespace

std::uint32_t intersect_feature_corner_edge(FeaturePrism& output, MaximumFeature& a, MaximumFeature& b,
                                            std::size_t corner, std::size_t edge, bool a_is_first) {
    const std::size_t ao = a_is_first ? 0 : 64;
    const std::size_t bo = a_is_first ? 64 : 0;
    const std::size_t previous = corner != 0 ? corner - 1 : static_cast<std::size_t>(a[140]) - 1;
    S3_ASSERT(corner < 8 && edge < 8 && previous < 8);
    const V bd = load(b, 8 + edge * 16);
    std::size_t selected = previous;
    auto nearly_parallel = [&](std::size_t index) {
        return rs::from_bits(0x3d4ccccdu) > rs::abs(dot(load(a, 12 + index * 16), bd));
    };
    bool parallel;
    if (nearly_parallel(previous)) {
        parallel = true;
    } else {
        selected = corner;
        parallel = nearly_parallel(corner);
    }
    const V b_origin = load(b, 4 + edge * 16);
    if (!parallel) {
        a[0] = static_cast<std::uint32_t>(corner) * 2 + 1;
        b[0] = (static_cast<std::uint32_t>(edge) + 1) * 2;
        const V origin = load(a, 4 + corner * 16);
        float position = dot(bd, sub(origin, b_origin));
        if (position < rs::f32::MIN_POSITIVE) {
            b[0] = b[0] - 1u;
            position = 0.0f;
        } else {
            const V length = load(b, 16 + edge * 16);
            bool all = true;
            for (float v : length) all = all && position > v;
            if (all) {
                b[0] = b[0] + 1u;
                position = length[0];
            }
        }
        store(output, ao, origin);
        store(output, bo, madd(bd, position, b_origin));
        return 1;
    }
    a[0] = (static_cast<std::uint32_t>(selected) + 1) * 2;
    b[0] = (static_cast<std::uint32_t>(edge) + 1) * 2;
    const V a_origin = load(a, 4 + selected * 16);
    const V ad = load(a, 8 + selected * 16);
    const float distance = dot(ad, sub(b_origin, a_origin));
    if (distance < rs::f32::MIN_POSITIVE) {
        store(output, ao, a_origin);
        store(output, bo, b_origin);
        a[0] = a[0] - 1u;
        b[0] = b[0] - 1u;
        return 1;
    }
    const V al = load(a, 16 + selected * 16);
    const V bl = load(b, 16 + edge * 16);
    bool both = true;
    for (std::size_t i = 0; i < 4; ++i) both = both && rs::f32::MIN_POSITIVE > (al[i] + bl[i]) - distance;
    if (both) {
        store(output, ao, end(a, selected));
        store(output, bo, end(b, edge));
        a[0] = a[0] + 1u;
        b[0] = b[0] + 1u;
        return 1;
    }
    bool a_all = true;
    for (float v : al) a_all = a_all && rs::f32::MIN_POSITIVE > v - distance;
    if (a_all) {
        store(output, ao, end(a, selected));
        V scaled;
        for (std::size_t i = 0; i < 4; ++i) scaled[i] = bd[i] * (al[i] - distance);
        store(output, bo, sub(b_origin, scaled));
    } else {
        store(output, ao, madd(ad, distance, a_origin));
        store(output, bo, b_origin);
    }
    bool b_all = true;
    for (float v : bl) b_all = b_all && rs::f32::MIN_POSITIVE > v - distance;
    if (b_all) {
        V scaled;
        for (std::size_t i = 0; i < 4; ++i) scaled[i] = ad[i] * (bl[i] - distance);
        store(output, ao + 4, sub(a_origin, scaled));
        store(output, bo + 4, end(b, edge));
    } else {
        store(output, ao + 4, a_origin);
        store(output, bo + 4, madd(bd, distance, b_origin));
    }
    return 2;
}

}  // namespace s3::physics::world_contact::face_edge
