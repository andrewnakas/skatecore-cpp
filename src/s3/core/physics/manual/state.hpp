// Port of skate-core/src/physics/manual/state.rs.
// The five board-owned persistent fields used by CalcManualEffect.
#pragma once
#include <cstdint>

#include "s3/core/rs.hpp"

namespace s3::physics::manual::state {

/// Explicit dependency returned by the entry-state slice, never a no-op
/// replacement for Ground's call to RemoveVelIntoGroundForManual 82D37960.
enum class [[nodiscard]] ManualEntryContinuation {
    Continue,
    RemoveVelocityIntoGround,
};

struct ManualState {
    /// Board+264: 0.95/0.05 filtered clamped angle error.
    float filtered_angle_error = 0.0f;
    /// Board+268: normalized requested angle for this tick.
    float target_angle = 0.0f;
    /// Board+272: normalized measured deck angle, retained for the next derivative.
    float measured_angle = 0.0f;
    /// Board+276: accumulated controller output, scaled to angular displacement.
    float angular_correction = 0.0f;
    /// Board+280: zero also identifies the first active controller tick.
    float elapsed = 0.0f;

    /// 82C04EEC..4EFC when ProcessedPhysIn+2720 equals either signed zero.
    /// This resets only these five controller fields, not heading state+284.
    void reset() {
        filtered_angle_error = 0.0f;
        target_angle = 0.0f;
        measured_angle = 0.0f;
        elapsed = 0.0f;
        angular_correction = 0.0f;
    }

    /// Manual-state slice of Ground Enter 82D376F4..7750. The caller still
    /// owns all other entry work, including the returned velocity operation.
    /// `previous_category` is ProcessedPhysIn+2516; 100 is the ground category.
    ManualEntryContinuation enter_ground(std::uint32_t previous_category,
                                         float powerslide_exit_scale) {
        if (previous_category == 100) {
            // +268 target, +272 measurement and +280 timer are preserved.
            angular_correction *= powerslide_exit_scale;
            filtered_angle_error *= powerslide_exit_scale;
            return ManualEntryContinuation::Continue;
        }
        // Ground's inline reset82D3770C..771C writes +276 before +280.
        filtered_angle_error = 0.0f;
        target_angle = 0.0f;
        measured_angle = 0.0f;
        angular_correction = 0.0f;
        elapsed = 0.0f;
        return ManualEntryContinuation::RemoveVelocityIntoGround;
    }
};
S3_DERIVE_EQ(ManualState, filtered_angle_error, target_angle, measured_angle, angular_correction,
             elapsed)

}  // namespace s3::physics::manual::state
