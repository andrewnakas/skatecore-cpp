// Port of skate-game/src/physics/ground_runtime/trajectory.rs.
// NOTE: this file is not mounted by ground_runtime/mod.rs in Rust (dead code); ported for completeness.
//! Ground ordinary tail82D749D0 cancels pending trajectory work and clears
//!published validity. Rust owns the request lifetime; no guest pool is copied.
#pragma once
#include <cstdint>
#include <optional>

namespace s3::game::physics::ground_runtime::trajectory {

///The caller's actual pending request type can cancel work on Drop. Prediction
///data may remain cached, as native Reset clears validity rather than erasing
///all output vectors. Lower six flags retain their existing values.
template <class T>
struct GroundTrajectoryState {
    std::optional<T> pending_request;
    bool primary_valid_288 = false;
    bool secondary_valid_592 = false;
    bool result_valid_9840 = false;
    std::uint8_t flags_12836 = 0;

    ///Publish the native cancellation after releasing the current request.
    void cancel() {
        pending_request.reset();
        result_valid_9840 = false;
        primary_valid_288 = false;
        secondary_valid_592 = false;
        flags_12836 &= 0x3f;
    }
};

}  // namespace s3::game::physics::ground_runtime::trajectory
