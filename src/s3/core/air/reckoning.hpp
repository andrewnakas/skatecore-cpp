// Port of skate-core/src/air/reckoning/mod.rs.
// Complete original TU3 Reckoning::UpdateAirStates82D8DBD8.
// Shared Ground filters, frame owners and BodySpin history persist across states.
#pragma once
#include <array>

#include "s3/core/air/body_spin.hpp"
#include "s3/core/air/reckoning/data.hpp"
#include "s3/core/air/reckoning/math.hpp"
#include "s3/core/math.hpp"
#include "s3/core/riding/ground_orientation.hpp"
#include "s3/core/riding/reckoning_frames.hpp"

namespace s3::air::reckoning {

// pub use data::{AirState, Input, Settings};
using data::AirState;
using data::Input;
using data::Settings;
// pub use math::limit_angle as clamp_vector_within_max_angle;
inline std::array<float, 4> clamp_vector_within_max_angle(std::array<float, 4> target, std::array<float, 4> from,
                                                          float maximum) {
    return math::limit_angle(target, from, maximum);
}

using body_spin::BodySpinState;
using riding::ground_orientation::GroundOrientation;
using riding::reckoning_frames::ReckoningFrames;

void update(GroundOrientation& orientation, ReckoningFrames& frames, BodySpinState& body_spin, AirState& state,
            const Settings& settings, const Input& input);

// Module-private in Rust; exposed because the ported tests use them.
inline std::array<float, 4> lanes(s3::math::Vector3 v) { return {v.x, v.y, v.z, 0.0f}; }
inline s3::math::Vector3 vector(std::array<float, 4> v) { return s3::math::Vector3::new_(v[0], v[1], v[2]); }

/// Handplant Reckoning82D8E1C0. Uses the ordinary persistent normal filters.
void update_plant(GroundOrientation& orientation, ReckoningFrames& frames, BodySpinState& body_spin, AirState& state,
                  const Settings& settings, std::array<float, 4> up, std::array<float, 4> heading, bool reverse);

}  // namespace s3::air::reckoning
