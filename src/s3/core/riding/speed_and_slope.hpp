// Port of skate-core/src/riding/speed_and_slope.rs.
// Speed/slope coefficient published to the stock ActionGraph.
//
// TU3 PhysicalPlayerHiLOD::CalculateHeadingAdjustFactor82DB5E10 is called
// during PostInput82DB56C0. ProcessOutput82DB7580 publishes its result to
// SkateboardMotion+192. The source uses the *turn torque* curves, despite
// the function name; the nearby HeadingAdjustVsSpeed/Slope curves differ.
#pragma once
#include "s3/core/point_graph.hpp"

namespace s3::riding::speed_and_slope {

using point_graph::PointGraph;

struct SpeedAndSlopeSettings {
    /// physics_heading layout336/368.
    PointGraph<8> turn_torque_vs_speed{};
    /// physics_heading layout400/432.
    PointGraph<8> turn_torque_vs_slope{};
    /// physics_heading layout600.
    float heading_adjust_max_speed = 0.0f;

    /// Complete82DB5E10, including the slope cutoff and speed saturation.
    ///
    /// `ground_normal_y` is ground output+96.Y, copied to ProcessedPhysIn+464;
    /// `forward_speed` is skateboard output+168, copied to ProcessedPhysIn+2612.
    /// The reciprocal/acos primitives retain their documented numerical
    /// approximation boundary until independent hardware validation.
    float calculate(float ground_normal_y, float forward_speed) const;
};
S3_DERIVE_EQ(SpeedAndSlopeSettings, turn_torque_vs_speed, turn_torque_vs_slope, heading_adjust_max_speed)

}  // namespace s3::riding::speed_and_slope
