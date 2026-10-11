// Port of skate-core/src/animation/playback_attributes.rs.
//! PhaseBlend attribute intersection82D161B0 and Scale/AddWeighted82D16370/428.
#pragma once
#include <string>
#include <utility>
#include <vector>

#include "s3/core/animation/output/attributes.hpp"
#include "s3/core/animation/slice.hpp"
#include "s3/core/rs.hpp"

namespace s3::animation::playback_attributes {

using output::attributes::AnimationAttribute;
using output::attributes::AttributeName;

/// Bone-name mapping supplied by the stock hierarchy. A native excluded bone
/// (mirror index -1) maps to itself; missing names remain a data error.
/// Rust tuple struct `AttributeMirror(pub Vec<(AttributeName, AttributeName)>)`.
struct AttributeMirror {
    std::vector<std::pair<AttributeName, AttributeName>> _0;

    /// Attribute::Mirror82530F48 changes payload identity, never the event key.
    rs::Result<rs::Unit, std::string> apply(AnimationAttribute& attribute) const;
};

rs::Result<rs::Unit, std::string> blend(AnimationAttribute& left, const AnimationAttribute& right, float weight);

///82D16370. Keep reference names and event flags; scale only numeric payload.
rs::Result<rs::Unit, std::string> scale(AnimationAttribute& attribute, float weight);

///82D16428. Time adds are separate instructions; numeric payload uses FMA.
rs::Result<rs::Unit, std::string> add_weighted(AnimationAttribute& left, const AnimationAttribute& right,
                                               float weight);

/// Native uses a forward-only intersection, retaining left order/metadata.
/// It does not append right-only attributes or replace the left reference name.
rs::Result<std::vector<AnimationAttribute>, std::string> intersection(std::vector<AnimationAttribute> left,
                                                                       Slice<const AnimationAttribute> right,
                                                                       float weight);

}  // namespace s3::animation::playback_attributes
