// Port of skate-core/src/player/offboard/twist_lean.rs (whole file).
// MatchTwistAndLean: TU3 Begin82BC0848, Update82BC0860, read82BC0968.
#pragma once
#include <optional>

#include "s3/core/rs.hpp"

namespace s3::player::offboard::twist_lean {

struct Observation {
    /// Actual PhysOutSkeleton+496 and+500, produced by active-state Fill.
    float hips_right_angle = 0.0f;
    float hips_up_angle = 0.0f;
    /// Live ISkaterAnim virtual28, not a physical stance snapshot.
    bool mirrored = false;
};
S3_DERIVE_EQ(Observation, hips_right_angle, hips_up_angle, mirrored)

struct State;
inline State read(Observation observation);

struct State {
    float twist = 0.0f;
    float lean = 0.0f;

    /// Instance constructor82BA8DC0 initializes both retained values to zero.
    /// An absent original component makes read82BC0968 leave them unchanged.
    void begin(std::optional<Observation> observation) {
        if (observation) *this = read(*observation);
    }
    /// The default publishes captured values; exactly update="always" rereads.
    /// The live branch initializes its local values to zero before the read.
    State update(bool always, std::optional<Observation> observation) const {
        if (always) return observation ? read(*observation) : State{};
        return *this;
    }
};
S3_DERIVE_EQ(State, twist, lean)

inline State read(Observation observation) {
    return State{observation.mirrored ? -observation.hips_up_angle : observation.hips_up_angle,
                 observation.hips_right_angle};
}

}  // namespace s3::player::offboard::twist_lean
