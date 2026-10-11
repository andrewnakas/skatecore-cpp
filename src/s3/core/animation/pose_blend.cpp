// Port of skate-core/src/animation/pose_blend.rs.
#include "s3/core/animation/pose_blend.hpp"

#include "s3/core/physics/native_arithmetic.hpp"

namespace s3::animation::pose_blend {

using physics::native_arithmetic::dot4;
using physics::native_arithmetic::reciprocal_square_root_estimate;
using rs::mul_add;

namespace {
std::array<float, 4> interpolate(const std::array<float, 4>& first, const std::array<float, 4>& second,
                                 float weight) {
    std::array<float, 4> out{};
    for (std::size_t lane = 0; lane < 4; ++lane) out[lane] = mul_add(second[lane] - first[lane], weight, first[lane]);
    return out;
}
float refine(float squared) {
    float inverse = reciprocal_square_root_estimate(squared);
    for (int i = 0; i < 2; ++i) {
        inverse = mul_add(inverse * 0.5f, mul_add(-squared, inverse * inverse, 1.0f), inverse);
    }
    return inverse;
}
}  // namespace

rs::Result<rs::Unit, PoseBufferError> blend(Slice<const Sqt> first, Slice<const Sqt> second, float weight,
                                            Slice<Sqt> output) {
    if (first.size() != second.size()) {
        return rs::make_err(PoseBufferError(PoseBufferError::ShortInput{}));
    }
    if (output.size() < first.size()) {
        return rs::make_err(PoseBufferError(PoseBufferError::ShortOutput{}));
    }
    for (std::size_t i = 0; i < first.size(); ++i) {
        output[i] = blend_sample(first[i], second[i], weight);
    }
    return rs::Unit{};
}

Sqt blend_sample(Sqt first, Sqt second, float weight) {
    const bool positive = dot4(first.rotation, second.rotation) > 0.0f;
    std::array<float, 4> rotation{};
    for (std::size_t lane = 0; lane < 4; ++lane) {
        rotation[lane] = positive
                             ? mul_add(second.rotation[lane] - first.rotation[lane], weight, first.rotation[lane])
                             : mul_add(-(second.rotation[lane] + first.rotation[lane]), weight, first.rotation[lane]);
    }
    const float squared = dot4(rotation, rotation);
    const float inverse = refine(squared);
    Sqt out;
    out.scale = interpolate(first.scale, second.scale, weight);
    for (std::size_t lane = 0; lane < 4; ++lane) out.rotation[lane] = rotation[lane] * inverse;
    out.translation = interpolate(first.translation, second.translation, weight);
    return out;
}

Sqt channel_blend_sample(Sqt first, Sqt second, float weight, bool use_first_weights) {
    const float channel = use_first_weights ? first.translation[3] : second.translation[3];
    float coefficient = weight * channel;
    coefficient = coefficient > 1.0f ? 1.0f : coefficient;
    coefficient = 0.0f > coefficient ? 0.0f : coefficient;
    return blend_sample(first, second, coefficient);
}

rs::Result<std::vector<Sqt>, PoseBufferError> weighted(Slice<const std::vector<Sqt>> poses,
                                                       Slice<const float> weights) {
    if (poses.is_empty()) {
        return rs::make_err(PoseBufferError(PoseBufferError::ShortInput{}));
    }
    const std::vector<Sqt>& first = poses[0];
    bool mismatched = poses.size() != weights.size();
    for (const auto& p : poses) mismatched = mismatched || p.size() != first.size();
    if (mismatched) {
        return rs::make_err(PoseBufferError(PoseBufferError::ShortInput{}));
    }
    for (float weight : weights) {
        if (!rs::is_finite(weight)) return rs::make_err(PoseBufferError(PoseBufferError::NonFiniteWeight{}));
    }
    std::vector<Sqt> output;
    output.reserve(first.size());
    for (const Sqt& s : first) {
        Sqt o;
        for (std::size_t lane = 0; lane < 4; ++lane) {
            o.scale[lane] = s.scale[lane] * weights[0];
            o.rotation[lane] = s.rotation[lane] * weights[0];
            o.translation[lane] = s.translation[lane] * weights[0];
        }
        output.push_back(o);
    }
    for (std::size_t p = 1; p < poses.size(); ++p) {
        const float weight = weights[p];
        const std::vector<Sqt>& pose = poses[p];
        for (std::size_t i = 0; i < output.size() && i < pose.size(); ++i) {
            Sqt& out = output[i];
            const Sqt& s = pose[i];
            for (std::size_t lane = 0; lane < 4; ++lane) {
                out.scale[lane] = mul_add(s.scale[lane], weight, out.scale[lane]);
                out.rotation[lane] = mul_add(s.rotation[lane], weight, out.rotation[lane]);
                out.translation[lane] = mul_add(s.translation[lane], weight, out.translation[lane]);
            }
        }
    }
    for (Sqt& out : output) {
        const float squared = dot4(out.rotation, out.rotation);
        if (!rs::is_finite(squared) || squared <= 0.0f) {
            return rs::make_err(PoseBufferError(PoseBufferError::InvalidQuaternion{}));
        }
        const float inverse = refine(squared);
        for (float& v : out.rotation) v = v * inverse;
        for (float v : out.rotation) {
            if (!rs::is_finite(v)) return rs::make_err(PoseBufferError(PoseBufferError::InvalidQuaternion{}));
        }
    }
    return output;
}

}  // namespace s3::animation::pose_blend
