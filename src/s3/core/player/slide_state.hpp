// Port of skate-core/src/player/slide_state/mod.rs (whole file).
// PhysicsSlideGround101: Enter82D3A700, Exit82D3A828 and board82D3A900.
#pragma once
#include "s3/core/player/slide_state/forces.hpp"

namespace s3::player::slide_state {

using forces::angular_correction;
using forces::SlideInput;
using forces::SlideSettings;
using forces::SlideSurface;
using forces::sliding_force;

struct SlideState {
    float start_speed = 0.0f;
    float steering_push = 1.0f;
    float damped_turn = 0.0f;
    bool flag48 = false;
    bool wall_riding = false;

    /// Host storage; Enter defines every represented native field before use.
    static SlideState new_() { return SlideState{}; }
    void enter(float speed) {
        start_speed = speed;
        exit();
    }
    void exit() {
        flag48 = false;
        wall_riding = false;
        steering_push = 1.0f;
        damped_turn = 0.0f;
    }
};

}  // namespace s3::player::slide_state
