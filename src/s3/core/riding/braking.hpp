// Port of skate-core/src/riding/braking.rs.
// TU3 ground braking and linear drag, using the existing ground-force
// direct82D94AE0/82D94A20 exports and SKATE3_MAIN_SKATER_FUNCTIONS.md.
// Cached ground settings are explicit inputs; no replacement tuning defaults.
#pragma once
#include <cstdint>

#include "s3/core/math.hpp"
#include "s3/core/physics/force_queue.hpp"

namespace s3::riding::braking {

using math::Vector3;
using physics::force_queue::QueuedPointForce;

struct BrakeSettings {
    /// Cached settings +20, multiplied by processed input +2728.
    float input_force = 0.0f;
    /// Cached settings +24. The second request overrides the first.
    float override_force = 0.0f;
    /// Cached settings +28, compared against processed absolute speed +2616.
    float minimum_speed = 0.0f;
};

struct BrakeInput {
    std::uint32_t flags_2468 = 0;
    float input_2728 = 0.0f;
    float signed_speed = 0.0f;
    float absolute_body_speed = 0.0f;
    /// Selected surface collection's layout +264.
    float surface_factor = 0.0f;
    /// Processed vector +368, used without normalization.
    Vector3 direction;
};

/// Toolkit_CalcBraking 82D94AE0. GroundUpdate appends even a zero result
/// with tag 2 at 82D391A0. This result is force, not a velocity assignment.
QueuedPointForce calculate_braking(BrakeInput input, BrakeSettings settings);

struct LinearDragSettings {
    /// Cached +1244; multiplied by native constant 2.0 at 82060C50.
    float brake_speed = 0.0f;
    /// Cached +1248.
    float balance_speed = 0.0f;
    /// Cached +1252. Original semantic name remains unresolved.
    float comparison_threshold = 0.0f;
    /// Cached +1256.
    float balance_drag = 0.0f;
};

struct LinearDragInput {
    std::uint32_t flags_2468 = 0;
    float absolute_body_speed = 0.0f;
    float balance_2720 = 0.0f;
    float scalar_2724 = 0.0f;
    /// Floating argument f1 supplied by GroundUpdate, distinct from speed.
    float comparison_scalar = 0.0f;
};

/// Toolkit_CalcLinearDrag 82D94A20, including its low-speed brake override.
/// Applying the result to inertia is a separate caller operation.
float calculate_linear_drag(LinearDragInput input, LinearDragSettings settings);

}  // namespace s3::riding::braking
