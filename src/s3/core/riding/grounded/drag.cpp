// Port of skate-core/src/riding/grounded/drag.rs.
#include "s3/core/riding/grounded/drag.hpp"

namespace s3::riding::grounded::drag {

float GroundDragInput::calculate(LinearDragSettings settings) const {
    braking::LinearDragInput input;
    input.flags_2468 = flags_2468;
    input.absolute_body_speed = absolute_body_speed_2616;
    input.balance_2720 = balance_2720;
    input.scalar_2724 = scalar_2724;
    input.comparison_scalar = ground_normal_y;
    return braking::calculate_linear_drag(input, settings);
}

rs::Result<rs::Unit, DragBindingError> BodyInertias::set_linear_drag(float drag, DragSelection selection) {
    physics::Slice<const std::size_t> selected = part_inertia_indices;
    if (const auto* part = std::get_if<DragSelection::Part>(&selection.value)) {
        // `get(part..part.saturating_add(1))` filtered to exactly one element.
        const std::size_t start = part->_0;
        if (start >= part_inertia_indices.size()) {
            return rs::make_err(DragBindingError::PartOutsideAssembly);
        }
        selected = physics::Slice<const std::size_t>(part_inertia_indices.ptr + start, 1);
    }
    for (const std::size_t index : selected) {
        if (index >= inertias.size()) {
            return rs::make_err(DragBindingError::InertiaOutsideStorage);
        }
    }
    const std::size_t groups = selected.size() / 4;
    for (std::size_t g = 0; g < groups; ++g) {
        const std::size_t first = selected[4 * g];
        const std::size_t second = selected[4 * g + 1];
        const std::size_t third = selected[4 * g + 2];
        const std::size_t fourth = selected[4 * g + 3];
        const float coefficient = drag * DRAG_FREQUENCY;
        inertias[first].linear_drag = coefficient;
        inertias[second].linear_drag = coefficient;
        inertias[third].linear_drag = coefficient;
        inertias[fourth].linear_drag = coefficient;
    }
    for (std::size_t n = groups * 4; n < selected.size(); ++n) {
        inertias[selected[n]].linear_drag = drag * DRAG_FREQUENCY;
    }
    return rs::Unit{};
}

rs::Result<rs::Unit, DragBindingError> BodyInertias::apply_ground_drag(float calculated_drag) {
    return set_linear_drag(calculated_drag, DragSelection::AllParts{});
}

}  // namespace s3::riding::grounded::drag
