// Port of skate-core/src/riding/grounded/propulsion.rs.
// Brake/push production82D38EB4..8F08 and submission82D39194..91B0.
// These are two distinct slices of Ground UpdateSkateboard, not an entire
// ground update: intervening native force/manual producers must execute.
#pragma once
#include <array>
#include <cstdint>
#include <variant>

#include "s3/core/math.hpp"
#include "s3/core/physics/force_queue.hpp"
#include "s3/core/physics/manual/controller.hpp"
#include "s3/core/riding/braking.hpp"
#include "s3/core/riding/push.hpp"

namespace s3::riding::grounded::propulsion {

using braking::BrakeSettings;
using math::Vector3;
using physics::force_queue::BoardForceQueue;
using physics::force_queue::QueuedPointForce;
using physics::manual::controller::ManualEffect;
using push::PushAcceleration;

/// Actual ProcessedPhysIn fields consumed by these two native calculations.
/// No raw button mapping, recomputed mass, normalized axis or host timestep.
struct GroundPropulsionInput {
    std::uint32_t flags_2468 = 0;
    std::uint32_t flags_2472 = 0;
    ///+2608, produced by Skeleton's push attribute path82BDAC24/2C.
    float target_speed = 0.0f;
    ///+2612, copied by ProcessInput82DB4878 from player1800->4 field168.
    float signed_speed = 0.0f;
    ///+2616, separate from the signed projection above.
    float absolute_body_speed = 0.0f;
    ///+2660, consumed directly. Existing mass-producer attribution82C0140C
    ///still needs a fresh containing-body audit; do not recompute here.
    float scalar_2660 = 0.0f;
    ///+2604, unlike Ground's fixed pumping timestep.
    float timestep = 0.0f;
    ///+2728, Skeleton attribute scalar consumer82BDB0E8.
    float brake_input = 0.0f;
    ///+352 and+368 are distinct effective axes, used verbatim.
    Vector3 push_direction;
    Vector3 brake_direction;
    /// Processed+2544 -> collection+4 -> layout+264.
    float surface_braking_factor = 0.0f;
};

struct GroundPropulsionSettings {
    /// Ground+32 toolkit offsets20/24/28, cached by82D94D08/1C/34.
    BrakeSettings braking;
    /// Toolkit+16 from physics_push layout+8 at82D94CF4.
    float maximum_pushable_speed = 0.0f;
    /// Processed+2548 -> selected collection+4 -> layout+48/+52.
    /// These are separate from the pumping fields at that mode binding.
    std::array<float, 2> mode_speed_changes{};
};

/// Rust `enum PropulsionSubmission { BrakingAndPush([bool; 2]), ManualCorrection(bool) }`.
struct PropulsionSubmission {
    struct BrakingAndPush {
        std::array<bool, 2> _0{};
    };
    struct ManualCorrection {
        bool _0 = false;
    };
    std::variant<BrakingAndPush, ManualCorrection> value;

    PropulsionSubmission(BrakingAndPush v) : value(v) {}
    PropulsionSubmission(ManualCorrection v) : value(v) {}
};
S3_DERIVE_EQ(PropulsionSubmission::BrakingAndPush, _0)
S3_DERIVE_EQ(PropulsionSubmission::ManualCorrection, _0)
S3_DERIVE_EQ(PropulsionSubmission, value)

/// Native stack results retained until the later force-submission phase.
struct GroundPropulsion {
    QueuedPointForce braking;
    PushAcceleration push;

    /// Call at82D39194 after native correction82D39338 and any tag4/5 writes,
    /// before angular changes/tag6/1/8/16. Queues both records, even zero forces.
    /// A true actual CalcManualEffect output80 selects the alternate tag7 path
    /// and queues its corrective force+48/point+64 instead. Output81 is not
    /// this selector.82D94A20 writes
    /// no r9, preserving the output80 byte through the intervening drag call.
    /// Returns each append result; the native21-entry limit may drop a record.
    PropulsionSubmission submit(const ManualEffect& manual, BoardForceQueue& queue) const;
};

/// Run only after the earlier velocity/collision branches select82D38EB4.
/// The force helpers run even if the later manual branch suppresses submission.
/// Publishes Ground+2730 after push calculation, including clearing old state.
GroundPropulsion calculate(GroundPropulsionInput input, GroundPropulsionSettings settings,
                           std::uint8_t& push_suppressed_2730);

}  // namespace s3::riding::grounded::propulsion
