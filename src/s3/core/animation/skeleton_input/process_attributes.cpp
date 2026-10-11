// Port of skate-core/src/animation/skeleton_input/process_attributes.rs.
#include "s3/core/animation/skeleton_input/process_attributes.hpp"

#include "s3/core/animation/skeleton_input/catalog.hpp"

namespace s3::animation::skeleton_input::process_attributes {

using attribute_finalization::Accumulator;
using scalar_attributes::DispatchError;

rs::Result<rs::Unit, std::string> process(
    Slice<const output::attributes::AnimationAttribute> attributes,
    const contact_events::ContactEventPose& pose, scalar_attributes::ScalarAttributeInputs& fields,
    extended_attributes::ExtendedAttributes& extra, contact_events::ContactEventState& contacts,
    attribute_finalization::JumpAttributeState& cached_jump, scalar_attributes::AnimationControlOutput& output,
    attribute_finalization::FinalizationInput finalization, input::controller::ActionMap* input_map) {
    Accumulator accumulator = Accumulator::new_();
    for (std::size_t index = 0; index < attributes.size(); ++index) {
        const auto& attribute = attributes[index];
        auto result = [&]() -> rs::Result<rs::Unit, std::string> {
            if (attribute.kind == 3) {
                return contact_events::dispatch(attribute, pose, fields, contacts);
            }
            auto dispatched = scalar_attributes::dispatch_attribute(attribute, fields, output);
            if (dispatched.is_ok()) return rs::Unit{};
            const DispatchError& error = dispatched.unwrap_err();
            if (!std::get_if<DispatchError::KnownScalarUnavailable>(&error.value)) {
                return rs::make_err("Invalid scalar payload: " + scalar_attributes::debug_string(error));
            }
            const auto* entry = catalog::lookup(attribute.name);
            if (!entry) S3_PANIC("called `Option::unwrap()` on a `None` value");
            const std::string_view name = entry->name;
            const extended_attributes::ScalarFn scalar = [&]() -> rs::Result<float, std::string> {
                if (!attribute.payload._0[0]) {
                    return rs::make_err(std::string(name) + ": scalar payload absent");
                }
                return rs::from_bits(*attribute.payload._0[0]);
            };
            auto handled = accumulator.dispatch(name, scalar, fields, cached_jump, input_map);
            if (handled.is_err()) return rs::make_err(handled.unwrap_err());
            if (handled.unwrap()) return rs::Unit{};
            auto extended = extended_attributes::dispatch(name, scalar, fields, extra, output);
            if (extended.is_err()) return rs::make_err(extended.unwrap_err());
            if (extended.unwrap()) return rs::Unit{};
            return rs::make_err("Unimplemented known Skeleton attribute " + std::string(name));
        }();
        if (result.is_err()) {
            return rs::make_err("Skeleton attribute" + std::to_string(index) + ": " + result.unwrap_err());
        }
    }
    accumulator.finish(fields, extra, cached_jump, finalization, input_map);
    return rs::Unit{};
}

}  // namespace s3::animation::skeleton_input::process_attributes
