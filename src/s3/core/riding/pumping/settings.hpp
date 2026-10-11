// Port of skate-core/src/riding/pumping/settings.rs.
// Stock layout bindings consumed by TU3 Pumping Update/Calculate.
#pragma once
#include "s3/core/point_graph.hpp"

namespace s3::riding::pumping::settings {

using point_graph::PointGraph;

struct PumpingSettings {
    /// physics_pumping +16/+48 (the graph's four header floats are skipped).
    PointGraph<8> pump_vs_speed{};
    /// +96/+128, also skipping the PointNegGraphData8 header.
    PointGraph<8> pump_vs_time{};
    /// +160/+192, +224/+256, +288/+320.
    PointGraph<8> min_crouch_vs_ground_angle{};
    PointGraph<8> compression_vs_ground_angle{};
    PointGraph<8> compression_vs_deck_angle{};
    /// +364 PumpEffectDamping; +372 MinChangeInCOMBeforePumping.
    float height_change_damping = 0.0f;
    float minimum_height_change = 0.0f;
    /// +376 MaxDeltaHeightAllowedPerFrame.
    float maximum_height_change = 0.0f;
    /// +380 CompressionGroundScalar; +384 CompressionDeckScalar.
    float ground_compression_scale = 0.0f;
    float deck_compression_scale = 0.0f;
    /// +392 AngularSpeedDamping.
    float angular_speed_damping = 0.0f;
};

/// Ground 82D37E74 passes ProcessedPhysIn+2548, then Update loads wrapper+4.
/// ProcessInput 82DB4420..446C selects that wrapper from the five player modes.
struct PumpingMode {
    /// +12 Hash_9D1AEE3D7D8A4A7. Name describes its use, not a recovered name.
    float maximum_absorption_per_second = 0.0f;
    /// +16 Hash_D77AFD320B6241C5. Multiplied by the pumping timestep.
    float maximum_acceleration_per_second = 0.0f;
    /// +20 PumpEffectFactorAbsorption; +24 PumpEffectFactor.
    float absorption_factor = 0.0f;
    float acceleration_factor = 0.0f;
};

}  // namespace s3::riding::pumping::settings
