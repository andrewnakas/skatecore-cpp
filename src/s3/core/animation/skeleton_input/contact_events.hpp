// Port of skate-core/src/animation/skeleton_input/contact_events.rs.
// Complete kind3/status&12 dispatch82BDAAA0..82BDACE4.
// Bone identity is resolved even for an unknown event name. Push speed comes
// from the evaluated trajectory displacement, not a button-driven force.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

#include "s3/core/animation/output/attributes.hpp"
#include "s3/core/animation/output/native.hpp"
#include "s3/core/animation/skeleton_input/scalar_attributes.hpp"
#include "s3/core/animation/slice.hpp"

namespace s3::animation::skeleton_input::contact_events {

using output::NativeMatrix;
using output::attributes::AnimationAttribute;
using output::attributes::AttributeName;
using scalar_attributes::ScalarAttributeInputs;

struct ContactEventState {
    /// ProcessedPhysIn2560, including native GetBone's -1 result.
    std::int32_t bone = 0;
    /// ProcessedPhysIn2608, consumed by the native push-force calculation.
    float push_speed = 0.0f;
};
S3_DERIVE_EQ(ContactEventState, bone, push_speed)

struct ContactEventPose {
    Slice<const AttributeName> bone_names;
    Slice<const NativeMatrix> hierarchy;
    /// Skeleton11716, assigned by SkeletonData initialization.
    std::size_t trajectory_bone = 0;
    /// Skeleton4748: animation index of physical part19 (RIGHTTOEBASE).
    std::int32_t right_toe_bone = 0;
    /// Skeleton11704: current animation/physics input timestep.
    float timestep = 0.0f;
};

rs::Result<rs::Unit, std::string> dispatch(const AnimationAttribute& attribute, const ContactEventPose& pose,
                                           ScalarAttributeInputs& fields, ContactEventState& state);

}  // namespace s3::animation::skeleton_input::contact_events
