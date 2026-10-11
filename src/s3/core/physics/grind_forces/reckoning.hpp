// Port of skate-core/src/physics/grind_forces/reckoning.rs.
// UpdateGrindStates82D8E3E0, compared with S2 named82DDD2A8.
// Maintains CURRENT's shared filters/frames and BodySpin history; no second
// reckoning owner and no ground/air-state substitute.
#pragma once
#include "s3/core/air/body_spin.hpp"
#include "s3/core/air/reckoning.hpp"
#include "s3/core/physics/grind_forces.hpp"
#include "s3/core/riding/ground_orientation.hpp"
#include "s3/core/riding/reckoning_frames.hpp"

namespace s3::physics::grind_forces::reckoning {

struct Settings {
    V ground_normal_smoothing{};
    /// Stock schema: layout800 TiltVsRotGround,672 TiltVsSlopeGround.
    point_graph::PointGraph<8> tilt_vs_rotation{};
    point_graph::PointGraph<8> tilt_vs_slope{};
};

void update(riding::ground_orientation::GroundOrientation& orientation,
            riding::reckoning_frames::ReckoningFrames& frames, air::body_spin::BodySpinState& body_spin,
            air::reckoning::AirState& air, const Settings& settings, V normal, V heading, float smoothing,
            bool reverse_stance, float physical_body_spin);

}  // namespace s3::physics::grind_forces::reckoning
