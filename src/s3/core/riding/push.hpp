// Port of skate-core/src/riding/push.rs.
// TU3 Toolkit_CalcPushAcceleration (`0x82D948B8`). Despite the native name,
// the returned vector includes total body mass and is a force-queue payload.
// It is queued with tag 3; deck inverse mass is applied by `0x82C03718`.
#pragma once
#include <cstdint>

#include "s3/core/math.hpp"
#include "s3/core/physics/force_queue.hpp"

namespace s3::riding::push {

using math::Vector3;
using physics::force_queue::BoardForceQueue;
using physics::force_queue::QueuedPointForce;

struct PushInput {
    std::uint32_t flags_2468 = 0;
    std::uint32_t flags_2472 = 0;
    float target_speed = 0.0f;
    float current_speed = 0.0f;
    float absolute_body_speed = 0.0f;
    /// Input +2660, populated at 0x82C0140C by 0x82C06F58's sum of body
    /// masses. This is not a tunable push strength or direct velocity scale.
    float scale = 0.0f;
    float delta_seconds = 0.0f;
    Vector3 direction;
};

struct PushLimits {
    float maximum_pushable_speed = 0.0f;
    float low_speed_change = 0.0f;
    float high_speed_change = 0.0f;
};

struct PushAcceleration {
    Vector3 vector;
    Vector3 local_point;
    /// Native output byte; set only when push is requested and flag 0x200 is set.
    bool suppressed = false;
};
S3_DERIVE_EQ(PushAcceleration, vector, local_point, suppressed)

PushAcceleration calculate_acceleration(PushInput input, PushLimits limits);

/// Standalone convenience for callers that explicitly need a fresh mass sum.
/// Input +2660 is populated by 0x82C0140C, and 0x82D391B0 appends the result.
/// The input scale is overwritten with the live body mass sum. No direct
/// velocity adjustment is made; the force consumer and solver own its effect.
/// Ground82D38800 instead consumes the already-produced Processed+2660 and
/// defers submission until after the manual branch; use grounded::propulsion
/// for that caller. This convenience does not reproduce Ground's scheduling.
PushAcceleration enqueue_push(PushInput input, PushLimits limits, physics::Slice<const float> inverse_masses,
                              BoardForceQueue& queue);

}  // namespace s3::riding::push
