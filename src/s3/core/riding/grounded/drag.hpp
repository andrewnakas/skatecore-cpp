// Port of skate-core/src/riding/grounded/drag.rs.
// Ground drag calculation82D39100/104 and inertia publication82D392A8..B4.
// Body::SetLinearDrag82D9CD60 writes inertia fields, not current velocities.
#pragma once
#include <cstddef>
#include <cstdint>
#include <variant>

#include "s3/core/physics/rigid_body.hpp"
#include "s3/core/physics/slice.hpp"
#include "s3/core/riding/braking.hpp"

namespace s3::riding::grounded::drag {

using braking::LinearDragSettings;
using physics::rigid_body::RetailInertiaDynamics;

/// Literal at822F860C, verified in the original TU3 image. It is not replaced
/// by60 or recomputed from the current simulation timestep.
inline constexpr float DRAG_FREQUENCY = rs::from_bits(0x426fffffu);

struct GroundDragInput {
    std::uint32_t flags_2468 = 0;
    float absolute_body_speed_2616 = 0.0f;
    float balance_2720 = 0.0f;
    float scalar_2724 = 0.0f;
    /// Ground context+1216 vector Y, passed in f1 at82D39100. This is not
    /// Processed speed, a manual output, or a timestep.
    float ground_normal_y = 0.0f;

    /// Calculate before the manual-output80 branch at82D39110. The returned
    /// drag must be retained across force submission, then applied afterward.
    float calculate(LinearDragSettings settings) const;
};

/// Rust `enum DragSelection { AllParts, Part(usize) }`.
struct DragSelection {
    /// Native r5=-1, used by the inspected Ground update and exit callsites.
    struct AllParts {};
    struct Part {
        std::size_t _0 = 0;
    };
    std::variant<AllParts, Part> value;

    DragSelection(AllParts v) : value(v) {}
    DragSelection(Part v) : value(v) {}
};
S3_DERIVE_EQ_EMPTY(DragSelection::AllParts)
S3_DERIVE_EQ(DragSelection::Part, _0)
S3_DERIVE_EQ(DragSelection, value)

enum class DragBindingError {
    PartOutsideAssembly,
    InertiaOutsideStorage,
};

/// Native body+24 assembly contains part count+28 and definitions+8; each
///96-byte part resolves body+76 -> inertia+92. Shared inertias remain shared:
/// this binding references the live dynamics objects, not copies per part.
struct BodyInertias {
    physics::Slice<const std::size_t> part_inertia_indices;
    physics::Slice<RetailInertiaDynamics> inertias;

    /// Exact valid-binding setter82D9CD60. Invalid host bindings are rejected
    /// before writes; native would address invalid storage rather than recover.
    /// All-parts order is0..count, with four resolved handles before each group
    /// of four writes and a remaining single-part tail. Only inertia+32 changes.
    rs::Result<rs::Unit, DragBindingError> set_linear_drag(float drag, DragSelection selection);

    /// Ground82D392B4: after either ordinary tag16 or alternate tag7 submission,
    /// apply the drag retained from82D39104 to all parts. The caller owns the
    /// remaining force-strength flag and ground-correction helpers afterward.
    rs::Result<rs::Unit, DragBindingError> apply_ground_drag(float calculated_drag);
};

}  // namespace s3::riding::grounded::drag
