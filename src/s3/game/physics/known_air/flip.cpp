// Port of skate-game/src/physics/known_air/flip.rs.
//! Original Reckoning ResetBodyFlip82D8E9E0 / BeginBodyFlip82D8EAB0.
#include "s3/game/physics/known_air/flip.hpp"

#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/game/physics.hpp"
#include "s3/game/physics/skater.hpp"

namespace s3::game::physics::known_air::runtime {

using namespace ::s3::game::physics::known_air::math;
using ::s3::physics::skeleton_animation_record::IDENTITY;

void Runtime::reset_flip() {
    auto& frames = physics.riding.reckoning_frames;
    const auto n = physics.riding.reckoning.up;
    const V up = rotate(frames.body_flip, V{n.x, n.y, n.z, 0.0f});
    physics.riding.reckoning.up = xyz(up);
    frames.heading = rotate(frames.body_flip, frames.heading);
    frames.body_flip = IDENTITY;
    auto& state = skater.air_reckoning.state;
    state.flip_active = false;
    state.flip_speed = 0.0f;
    state.flip_side = false;
    state.flip_angle = 0.0f;
}

void Runtime::begin_flip(bool side) {
    auto& state = skater.air_reckoning.state;
    if (state.flip_active) {
        return;
    }
    physics.riding.reckoning_frames.body_flip = IDENTITY;
    state.flip_speed = 0.0f;
    state.flip_angle = 0.0f;
    auto basis = physics.riding.reckoning_frames.system;
    if (toolkit.control_sign < 0.0f) {
        basis[0] = scale(basis[0], -1.0f);
        basis[2] = scale(basis[2], -1.0f);
    }
    const float v = skater.animation_input.fields.body_spin * 4.0f;
    const float lo = -v >= 0.0f ? 0.0f : v;
    const float blend = 1.0f - lo >= 0.0f ? lo : 1.0f;
    const V adjustment = scale(skater.known_air.flip_axis_adjustment, blend);
    V axis{};
    for (std::size_t i = 0; i < 4; ++i) {
        axis[i] = std::fma(basis[2][i], adjustment[2], std::fma(basis[1][i], adjustment[1], basis[0][i]));
    }
    state.flip_axis = normalized(axis).first;
    if (side) {
        state.flip_axis = scale(state.flip_axis, -1.0f);
    }
    state.flip_side = side;
    state.flip_active = true;
}

}  // namespace s3::game::physics::known_air::runtime
