// Port of skate-core/src/player/offboard/hippy_jump.rs (whole file).
// TU3 `Skateboard::Toolkit_CalcHippyJumpVel` at `0x82D79F00`.
//
// The helper constructs the rider trajectory velocity used by
// `PhysState_LandingOnDeck::InitTraj`; it does not write board-body velocity.
#pragma once
#include <array>

#include "s3/core/rs.hpp"

namespace s3::player::offboard::hippy_jump {

using Vector = std::array<float, 4>;

struct Input {
    float desired_height = 0.0f;
    Vector board_position{};
    Vector centre_of_mass_position{};
    Vector reference_up{};
    Vector current_velocity{};
};

namespace detail {
inline float dot3(Vector a, Vector b) { return rs::mul_add(a[0], b[0], rs::mul_add(a[1], b[1], a[2] * b[2])); }
inline Vector add(Vector a, Vector b) { return Vector{a[0] + b[0], a[1] + b[1], a[2] + b[2], a[3] + b[3]}; }
inline Vector sub(Vector a, Vector b) { return Vector{a[0] - b[0], a[1] - b[1], a[2] - b[2], a[3] - b[3]}; }
inline Vector scale(Vector v, float s) { return Vector{v[0] * s, v[1] * s, v[2] * s, v[3] * s}; }
}  // namespace detail

inline Vector calculate(const Input& input) {
    using namespace detail;
    const Vector up = input.reference_up;
    const Vector displacement = sub(input.centre_of_mass_position, input.board_position);
    const float current_height = dot3(up, displacement);
    const float vertical_velocity = dot3(up, input.current_velocity);
    const Vector planar_velocity = sub(input.current_velocity, scale(up, vertical_velocity));
    const float launch_speed = rs::sqrt((input.desired_height - current_height) * 19.6f);
    return add(planar_velocity, scale(up, launch_speed));
}

}  // namespace s3::player::offboard::hippy_jump
