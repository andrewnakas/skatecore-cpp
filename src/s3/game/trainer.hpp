// Port of skate-game/src/trainer.rs.
//! SIMREF: copy of skate_mods::TrainerTuning (skate-mods/src/schema.rs) without serde/mlua deps.
//! Mods are not loaded by the reference, so this always stays at Default (all 1.0, hold_fakie=false).
#pragma once
#include "s3/core/rs.hpp"

namespace s3::game::trainer {

struct TrainerTuning {
    float pop = 1.0f;
    float grind_pop = 1.0f;
    float push_speed = 1.0f;
    float push_power = 1.0f;
    float braking = 1.0f;
    float steering = 1.0f;
    float wobble = 1.0f;
    float offboard_jump = 1.0f;
    float grip = 1.0f;
    float turn_power = 1.0f;
    float manual_drag = 1.0f;
    bool hold_fakie = false;
};
S3_DERIVE_EQ(TrainerTuning, pop, grind_pop, push_speed, push_power, braking, steering, wobble, offboard_jump, grip,
             turn_power, manual_drag, hold_fakie)

}  // namespace s3::game::trainer
