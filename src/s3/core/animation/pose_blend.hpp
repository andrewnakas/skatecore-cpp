// Port of skate-core/src/animation/pose_blend.rs.
// Standard SQT blend in TU3 ACS828CB418..4A4 and worker828D6BA8..6C2C.
// The caller owns tree order and coefficients; interpolation does not flatten
// nested blends or use a renderer's quaternion blend implementation.
#pragma once
#include <vector>

#include "s3/core/animation/output.hpp"
#include "s3/core/animation/slice.hpp"

namespace s3::animation::pose_blend {

using output::PoseBufferError;
using output::Sqt;

rs::Result<rs::Unit, PoseBufferError> blend(Slice<const Sqt> first, Slice<const Sqt> second, float weight,
                                            Slice<Sqt> output);

Sqt blend_sample(Sqt first, Sqt second, float weight);

/// ACSChannelBlend828CC91C..CA64 selects the first subtree's translation.W
/// when requested, otherwise the second's, and clamps the weighted coefficient.
Sqt channel_blend_sample(Sqt first, Sqt second, float weight, bool use_first_weights);

///ACS828CBF68..828CC0B4: weighted SQT accumulation in authored order,
///then one quaternion normalization. This is distinct from repeated two-pose
///shortest-arc blending; the native multi-pose path does not flip hemispheres.
rs::Result<std::vector<Sqt>, PoseBufferError> weighted(Slice<const std::vector<Sqt>> poses,
                                                       Slice<const float> weights);

}  // namespace s3::animation::pose_blend
