// Port of skate-core/src/animation/playback_attributes.rs.
#include "s3/core/animation/playback_attributes.hpp"

#include <cstddef>
#include <cstdint>

namespace s3::animation::playback_attributes {

namespace {
using Status = rs::Result<rs::Unit, std::string>;

Slice<const std::size_t> numeric_lanes(std::uint8_t kind) {
    static const std::size_t scalar[] = {0};
    static const std::size_t vector[] = {0, 1, 2, 3};
    static const std::size_t six[] = {5};
    switch (kind) {
        case 0:
        case 2: return scalar;
        case 1: return vector;
        case 3: return six;
        default: return {};
    }
}
}  // namespace

Status AttributeMirror::apply(AnimationAttribute& attribute) const {
    switch (attribute.kind) {
        case 1: {
            const auto x = attribute.payload._0[0];
            if (!x) return rs::make_err(std::string("Uninitialized mirrored vector attribute"));
            attribute.payload._0[0] = *x ^ 0x80000000u;
            break;
        }
        case 3: {
            std::array<std::uint32_t, 5> name{};
            for (std::size_t i = 0; i < 5; ++i) {
                const auto w = attribute.payload._0[i];
                if (!w) return rs::make_err(std::string("Uninitialized mirrored bone reference"));
                name[i] = *w;
            }
            const AttributeName* mirrored = nullptr;
            for (const auto& entry : _0) {
                if (entry.first._0 == name) {
                    mirrored = &entry.second;
                    break;
                }
            }
            if (!mirrored) return rs::make_err(std::string("Animation event bone is absent from its hierarchy"));
            for (std::size_t i = 0; i < 5; ++i) attribute.payload._0[i] = mirrored->_0[i];
            break;
        }
        default: break;
    }
    return rs::Unit{};
}

Status blend(AnimationAttribute& left, const AnimationAttribute& right, float weight) {
    const float left_weight = 1.0f - weight;
    if ((left.status & 2) != 0) {
        left.begin_time = -1.0f;
        left.end_time = -1.0f;
    } else {
        // Time fields use separate multiply/add instructions, not fused math.
        left.begin_time = right.begin_time * weight + left.begin_time * left_weight;
        left.end_time = right.end_time * weight + left.end_time * left_weight;
    }
    for (std::size_t i : numeric_lanes(left.kind)) {
        const auto lw = left.payload._0[i];
        if (!lw) return rs::make_err(std::string("Uninitialized blended attribute payload"));
        const float l = rs::from_bits(*lw);
        const auto rw = right.payload._0[i];
        if (!rw) return rs::make_err(std::string("Uninitialized blended attribute payload"));
        const float r = rs::from_bits(*rw);
        left.payload._0[i] = rs::to_bits(rs::mul_add(r, weight, l * left_weight));
    }
    return rs::Unit{};
}

Status scale(AnimationAttribute& attribute, float weight) {
    if ((attribute.status & 2) != 0) {
        attribute.begin_time = -1.0f;
        attribute.end_time = -1.0f;
    } else {
        attribute.begin_time *= weight;
        attribute.end_time *= weight;
    }
    for (std::size_t i : numeric_lanes(attribute.kind)) {
        const auto w = attribute.payload._0[i];
        if (!w) return rs::make_err(std::string("Uninitialized weighted attribute"));
        const float value = rs::from_bits(*w);
        attribute.payload._0[i] = rs::to_bits(value * weight);
    }
    return rs::Unit{};
}

Status add_weighted(AnimationAttribute& left, const AnimationAttribute& right, float weight) {
    if ((left.status & 2) == 0) {
        left.begin_time += right.begin_time * weight;
        left.end_time += right.end_time * weight;
    }
    for (std::size_t i : numeric_lanes(left.kind)) {
        const auto aw = left.payload._0[i];
        if (!aw) return rs::make_err(std::string("Uninitialized weighted attribute"));
        const auto bw = right.payload._0[i];
        if (!bw) return rs::make_err(std::string("Uninitialized weighted attribute"));
        const float value = rs::mul_add(rs::from_bits(*bw), weight, rs::from_bits(*aw));
        left.payload._0[i] = rs::to_bits(value);
    }
    return rs::Unit{};
}

rs::Result<std::vector<AnimationAttribute>, std::string> intersection(std::vector<AnimationAttribute> left,
                                                                       Slice<const AnimationAttribute> right,
                                                                       float weight) {
    std::vector<AnimationAttribute> result;
    std::size_t next = 0;
    for (AnimationAttribute& attribute : left) {
        while (next < right.size() && right[next].name._0 < attribute.name._0) {
            next += 1;
        }
        const AnimationAttribute* other = right.get(next);
        if (!other) {
            break;
        }
        if (other->name == attribute.name) {
            auto r = blend(attribute, *other, weight);
            if (r.is_err()) return rs::make_err(r.unwrap_err());
            result.push_back(attribute);
        }
    }
    return result;
}

}  // namespace s3::animation::playback_attributes
