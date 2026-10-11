// Port of skate-core/src/riding/pumping/state.rs.
// Owned pumping fields. Layout offsets are evidence, not a runtime memory map.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/rs.hpp"

namespace s3::riding::pumping::state {

struct PumpingOutput {
    /// Skeleton output +512/+516/+520/+528/+532.
    float compression = 0.0f;
    float absorption = 0.0f;
    float ground_normal_absorption = 0.0f;
    float minimum_crouch = 0.0f;
    float deck_angle_absorption = 0.0f;
    /// Ground output +268/+324.
    float pump_acceleration = 0.0f;
    std::uint8_t intentional_pumping = 0;
};
S3_DERIVE_EQ(PumpingOutput, compression, absorption, ground_normal_absorption, minimum_crouch,
             deck_angle_absorption, pump_acceleration, intentional_pumping)

struct PumpingState {
    /// +0 and +16, cached after every successful Update, including the first.
    std::array<float, 4> previous_position{};
    std::array<float, 4> previous_normal{};
    /// +32/+36/+40.
    float smoothed_height_change = 0.0f;
    float pumping_time = 0.0f;
    float previous_height = 0.0f;
    /// +44/+48/+52.
    float pumping = 0.0f;
    float pump_acceleration = 0.0f;
    float angular_speed = 0.0f;
    /// +56/+60/+64/+68, also copied to physics output.
    float absorption = 0.0f;
    float ground_normal_absorption = 0.0f;
    float minimum_crouch = 0.0f;
    float deck_angle_absorption = 0.0f;
    /// +72 is reset but not read or written by Update/Calculate.
    float reset_only_scalar = 0.0f;
    /// +76 and +81 are the only bytes touched by these three functions.
    bool record_valid = false;
    std::uint8_t intentional_pumping = 0;

    /// TU3 82D8F1A0. Does not initialize bytes +77..80 or +82 onward; those
    /// are outside this owned state and must be preserved by any binary adapter.
    /// Ground Enter 82D378DC and Exit 82D37B58 both call this reset.
    void reset() { *this = reset_state(); }

    /// The output of the recovered reset, not a claim about the constructor.
    static constexpr PumpingState reset_state() { return PumpingState{}; }

    /// Inline FillPhysOut at 82DB6FA8..7000. Read after Update; force assembly
    /// consumes pump_acceleration earlier, within Ground Update 82D38800.
    PumpingOutput physics_output() const {
        PumpingOutput out;
        out.compression = 0.0f;
        out.absorption = absorption;
        out.ground_normal_absorption = ground_normal_absorption;
        out.minimum_crouch = minimum_crouch;
        out.deck_angle_absorption = deck_angle_absorption;
        out.pump_acceleration = pump_acceleration;
        out.intentional_pumping = intentional_pumping;
        return out;
    }
};
S3_DERIVE_EQ(PumpingState, previous_position, previous_normal, smoothed_height_change, pumping_time,
             previous_height, pumping, pump_acceleration, angular_speed, absorption, ground_normal_absorption,
             minimum_crouch, deck_angle_absorption, reset_only_scalar, record_valid, intentional_pumping)

}  // namespace s3::riding::pumping::state
