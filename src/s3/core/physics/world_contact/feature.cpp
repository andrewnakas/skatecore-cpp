// Port of skate-core/src/physics/world_contact/feature.rs.
#include "s3/core/physics/world_contact/feature.hpp"

#include "s3/core/physics/reciprocal_sqrt.hpp"
#include "s3/core/physics/world_contact/prism_math.hpp"

namespace s3::physics::world_contact::feature {

using reciprocal_sqrt::estimate;

namespace {
std::array<float, 4> load(const std::uint32_t* words, std::size_t offset) {
    return {rs::from_bits(words[offset]), rs::from_bits(words[offset + 1]), rs::from_bits(words[offset + 2]),
            rs::from_bits(words[offset + 3])};
}
}  // namespace

void initialize_feature_segment(std::uint32_t* output, std::array<std::uint32_t, 4> origin_bits,
                                std::array<std::uint32_t, 4> end_bits) {
    for (std::size_t i = 0; i < 4; ++i) output[i] = origin_bits[i];
    const auto origin = rs::from_bits(origin_bits);
    const auto end = rs::from_bits(end_bits);
    std::array<float, 4> delta;
    for (std::size_t i = 0; i < 4; ++i) delta[i] = end[i] - origin[i];
    const float squared = arithmetic::dot({delta[0], delta[1], delta[2]}, {delta[0], delta[1], delta[2]});
    // One rsqrt refinement, then one reciprocal refinement. Runtime initializer
    // 82F837D8 broadcasts 8212BF78 (34000000) to threshold vector830BDD40.
    float reciprocal = estimate(squared);
    reciprocal = rs::mul_add(reciprocal * 0.5f, rs::mul_add(-squared, reciprocal * reciprocal, 1.0f), reciprocal);
    float length;
    if (squared > rs::from_bits(0x34000000u)) {
        length = squared == 0.0f ? 0.0f : squared * reciprocal;
    } else {
        length = 0.0f;
    }
    for (std::size_t i = 12; i < 16; ++i) output[i] = rs::to_bits(length);
    float r = prism_math::reciprocal_estimate(length);
    r = rs::mul_add(r, rs::mul_add(-r, length, 1.0f), r);
    std::array<float, 4> direction{0.0f, 0.0f, 0.0f, 0.0f};
    if (squared > rs::from_bits(0x34000000u)) {
        for (std::size_t i = 0; i < 4; ++i) direction[i] = delta[i] * r;
    }
    for (std::size_t i = 0; i < 4; ++i) output[4 + i] = rs::to_bits(direction[i]);
}

void capsule_maximum_feature(const std::array<std::uint32_t, 48>& gp, std::array<std::uint32_t, 4> direction,
                             MaximumFeature& output, std::array<std::uint32_t, 16>& segment_scratch) {
    const auto axis = load(gp.data(), 16);
    const auto normal = rs::from_bits(direction);
    const float projection = arithmetic::dot({axis[0], axis[1], axis[2]}, {normal[0], normal[1], normal[2]});
    const float half = rs::from_bits(gp[28]);
    const auto center = load(gp.data(), 0);
    if (rs::abs(projection) < rs::from_bits(0x3d4ccccdu)) {
        std::array<std::uint32_t, 4> plus, minus;
        for (std::size_t i = 0; i < 4; ++i) {
            const float scaled = axis[i] * half;
            plus[i] = rs::to_bits(center[i] + scaled);
            minus[i] = rs::to_bits(center[i] - scaled);
        }
        initialize_feature_segment(segment_scratch, plus, minus);
        for (std::size_t i = 0; i < 16; ++i) output[4 + i] = segment_scratch[i];
        output[140] = 1;
    } else {
        std::array<float, 4> point;
        for (std::size_t i = 0; i < 4; ++i) {
            point[i] = projection > 0.0f ? rs::mul_add(axis[i], half, center[i]) : center[i] - axis[i] * half;
        }
        for (std::size_t i = 0; i < 4; ++i) output[136 + i] = rs::to_bits(point[i]);
        output[140] = 0;
    }
    output[0] = 0;
}

}  // namespace s3::physics::world_contact::feature
