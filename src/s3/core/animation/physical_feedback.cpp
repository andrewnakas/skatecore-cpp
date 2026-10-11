// Port of skate-core/src/animation/physical_feedback.rs.
#include "s3/core/animation/physical_feedback.hpp"

#include "s3/core/physics/native_arithmetic.hpp"

namespace s3::animation::physical_feedback {

namespace {
std::array<float, 4> vector(Vector3 value) { return {value.x, value.y, value.z, 0.0f}; }

float flush_subnormal(float value) {
    if ((rs::to_bits(value) & 0x7f800000u) == 0) {
        return rs::from_bits(rs::to_bits(value) & 0x80000000u);
    }
    return value;
}
}  // namespace

PhysicalFeedback publish(input::turn_conditioner::State& state, const input::turn_conditioner::Settings& settings,
                         const BoardMotionOutput& motion, const PumpingState& pumping,
                         const SpeedWobbleState& wobble, ReckoningFeedback reckoning, ControlFeedback controls,
                         ground_acceleration::Output acceleration) {
    namespace tc = input::turn_conditioner;
    const float lean = (controls.processed_flags & 0x00100000u) != 0 ? reckoning.target_lean_angle
                                                                     : -reckoning.target_lean_angle;
    tc::Input in;
    in.body_160 = motion.speed;
    // Skateboard::FillPhysOut82C03230..3238 reads Wobble+36.
    in.body_176 = rs::from_bits(wobble._0[5]);
    in.bundle_36_field_160 = lean;
    in.bundle_32_field_264 = controls.turn;
    in.animation_152 = std::uint8_t((controls.processed_flags & 0x80000000u) != 0);
    in.animation_156 = std::uint8_t(controls.animation_mirrored);
    const tc::Output turn = tc::update(state, in, settings);
    const std::array<float, 4> system = vector(reckoning.system_position);
    const std::array<float, 4> board = vector(reckoning.board_position);
    //82DB7A38..7A54: vsubfp followed by vmsum3fp. The isolated native
    // arithmetic retains its documented host approximation boundary.
    const std::array<float, 4> difference = rs::from_fn<4>([&](std::size_t lane) {
        return flush_subnormal(flush_subnormal(system[lane]) - flush_subnormal(board[lane]));
    });
    const float height = physics::native_arithmetic::dot3(difference, vector(reckoning.system_up));
    PhysicalFeedback out;
    out.turning.field_32 = turn[0];
    out.turning.field_36 = turn[1];
    out.turning.field_52 = turn[5];
    out.turning.field_56 = turn[6];
    out.turning.field_60 = turn[7];
    out.turning.body_168 = motion.forward_speed;
    out.crouching.body_84 = motion.linear_velocity.y;
    out.crouching.body_164 = motion.ground_speed;
    //82C02CF0 copies Pumping+44, named mPumpPotential in S2 FillPhysOut.
    out.crouching.body_188 = pumping.pumping;
    out.crouching.force_516 = pumping.absorption;
    out.crouching.ground_force_520 = pumping.ground_normal_absorption;
    out.crouching.minimum_crouch_528 = pumping.minimum_crouch;
    out.crouching.deck_angle_532 = pumping.deck_angle_absorption;
    out.crouching.animation_height_72 = height;
    out.pumping_acceleration = pumping.pump_acceleration;
    out.ground_acceleration = acceleration.acceleration;
    out.bumped = acceleration.bumped;
    out.conditioned_turn = turn;
    return out;
}

}  // namespace s3::animation::physical_feedback
