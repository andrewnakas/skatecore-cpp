// Port of skate-game/src/physics/grind/state.rs.
#include "s3/game/physics/grind/state.hpp"

namespace s3::game::physics::grind::state {

State State::new_() {
    State s;
    s.output = output::State{
        {0.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        0.0f,
        false,
        0,
        0,
        false,
        {0.0f, 0.0f, 0.0f, 0.0f},
        false,
        {0.0f, 0.0f, 0.0f, 0.0f},
        {false, false, false},
    };
    s.frame = {{
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 0.0f, 0.0f},
    }};
    s.already_jumped = false;
    s.updates = 0;
    s.leaving_updates = 0;
    s.preparing_jump = false;
    return s;
}

void State::enter(Family family, std::array<V, 4> board) {
    // Enter deliberately does not overwrite vectors48/64/80 or word104.
    output.substate = 1;
    output.leaving = false;
    already_jumped = false;
    output.just_jumped = false;
    output.tipslide_97_98_99 = {false, false, false};
    frame = board;
    updates = 0;
    output.crouch = 0.0f;
    leaving_updates = 0;
    output.jump_velocity = {0.0f, 0.0f, 0.0f, 0.0f};
    if (family == Family::Boardslide || family == Family::Darkslide) {
        // Derived Enter82D41100 clears bytes240/241, not impulse224.
        output.slide_wipeout = false;
        preparing_jump = false;
    }
}

}  // namespace s3::game::physics::grind::state
