// Port of skate-core/src/animation/skeleton_input/process_attributes.rs.
// Full ordered Skeleton::ProcessAnimAttributes dispatch82BDA0D0. The game
// supplies actual stock settings, pose and input map, and retains owned state.
#pragma once
#include <string>

#include "s3/core/animation/compat/input_controller.hpp"
#include "s3/core/animation/output/attributes.hpp"
#include "s3/core/animation/skeleton_input/attribute_finalization.hpp"
#include "s3/core/animation/skeleton_input/contact_events.hpp"
#include "s3/core/animation/skeleton_input/extended_attributes.hpp"
#include "s3/core/animation/skeleton_input/scalar_attributes.hpp"
#include "s3/core/animation/slice.hpp"

namespace s3::animation::skeleton_input::process_attributes {

/// `input_map` is Rust's `Option<&mut dyn ActionMap>` (nullptr = None).
rs::Result<rs::Unit, std::string> process(
    Slice<const output::attributes::AnimationAttribute> attributes,
    const contact_events::ContactEventPose& pose, scalar_attributes::ScalarAttributeInputs& fields,
    extended_attributes::ExtendedAttributes& extra, contact_events::ContactEventState& contacts,
    attribute_finalization::JumpAttributeState& cached_jump, scalar_attributes::AnimationControlOutput& output,
    attribute_finalization::FinalizationInput finalization, input::controller::ActionMap* input_map);

}  // namespace s3::animation::skeleton_input::process_attributes
