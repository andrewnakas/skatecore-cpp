// Port of skate-core/src/air/known/trajectory.rs.
// KnownAir trajectory setup, ground restoration, and follow update.
#pragma once
#include "s3/core/air/known/data.hpp"
#include "s3/core/air/known/runtime.hpp"

namespace s3::air::known::trajectory {

using data::KnownAirFrame;
using data::KnownAirModeSettings;
using data::KnownAirSettings;
using data::KnownAirState;
using data::KnownAirTrajectory;
using data::Vector4;
using runtime::KnownAirRuntime;

/// `PhysState_KnownAir::InitTrajectoryInfo`, TU3 `0x82D35508`.
void init_trajectory_info(KnownAirState& state, const KnownAirFrame& frame, const KnownAirSettings& settings,
                          const KnownAirModeSettings& mode, KnownAirRuntime& runtime);

/// Ground transition velocity restoration, TU3 `0x82D35998`.
void restore_velocity(KnownAirState& state, KnownAirFrame& frame, const KnownAirSettings& settings,
                      KnownAirRuntime& runtime);

/// `PhysState_KnownAir::UpdateTrajectoryFollow`, TU3 `0x82D36460`.
void update_trajectory_follow(KnownAirState& state, const KnownAirFrame& frame, const KnownAirSettings& settings,
                              const KnownAirRuntime& runtime);

// pub(crate) helpers.
Vector4 trajectory_position(const KnownAirTrajectory& trajectory, float time);
Vector4 trajectory_velocity(const KnownAirTrajectory& trajectory, float time);
inline Vector4 add(Vector4 left, Vector4 right) {
    return {left[0] + right[0], left[1] + right[1], left[2] + right[2], left[3] + right[3]};
}
inline Vector4 sub(Vector4 left, Vector4 right) {
    return {left[0] - right[0], left[1] - right[1], left[2] - right[2], left[3] - right[3]};
}
inline Vector4 mul_scalar(Vector4 vector, float scalar) {
    return {vector[0] * scalar, vector[1] * scalar, vector[2] * scalar, vector[3] * scalar};
}
inline float select(float test, float nonnegative, float negative) { return test >= 0.0f ? nonnegative : negative; }

}  // namespace s3::air::known::trajectory
