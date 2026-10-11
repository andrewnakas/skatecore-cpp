// Port of skate-core/src/physics/manual/settings.rs.
// physics_manual stock layout at global830CFDA4+200, wrapper+4.
#pragma once
#include "s3/core/point_graph.hpp"

namespace s3::physics::manual::settings {

using point_graph::PointGraph;

struct ManualGains {
    float proportional = 0.0f;
    float integral = 0.0f;
    float derivative = 0.0f;
};

struct ManualSettings {
    /// NoiseVsSpeed has a 16-byte header; X/Y arrays start at +96/+128.
    PointGraph<8> noise_vs_speed;
    /// +160 TorqueScalarWithNoContact; applies to emitted displacement, not state.
    float torque_scale_without_contact = 0.0f;
    /// +164 TorqueBleedOffWithNoContact, +172 StartTorqueScalar.
    float torque_bleed_without_contact = 0.0f;
    float start_torque_scale = 0.0f;
    /// +184 ProceduralNoiseScalar, +188 ProceduralNoiseFreq.
    float procedural_noise_scale = 0.0f;
    float procedural_noise_frequency = 0.0f;
    /// +200/204/208 PowerScalar_P/I/D; +232/236/240 ManualScalar_P/I/D.
    ManualGains powerslide;
    ManualGains manual;
    /// +216 MaxTiltAngle (degrees); +224 MaxAngleDiff (native radians).
    float maximum_tilt_degrees = 0.0f;
    float maximum_angle_error = 0.0f;
    /// +244 D_Scalar_Limit, +248 BrakeTiltAngle (degrees), +252 AnimationNoiseScalar.
    float derivative_limit = 0.0f;
    float brake_tilt_degrees = 0.0f;
    float animation_noise_scale = 0.0f;
};

struct ManualMode {
    /// physics_mode+32 Hash_84CB2F459F3FC811, compared to abs(local ground
    /// velocity Z). Legacy Rust field name; the source reads Processed416,
    /// not the angular velocity at720. Original stock name remains unresolved.
    float correction_angular_speed_threshold = 0.0f;
    /// physics_mode+36 Hash_F8CBC0F5FEF2240E, read as a byte.
    bool corrective_force_enabled = false;
};

}  // namespace s3::physics::manual::settings
