// Port of skate-game/src/physics/player_input/grind/history.rs.
#include "s3/game/physics/player_input/grind/history.hpp"

namespace s3::game::physics::player_input::grind {

using history::decrement;

void GrindInputState::permission(const ProcessedPhysicsInput& p, std::int32_t air_counter) {
    const std::uint32_t state = p.state_2508;
    if (state != previous_state) {
        std::uint32_t add = 0;
        switch (state) {
            case 400:
            case 402:
            case 404: add = 50; break;
            case 403: add = 20; break;
            default: add = 0; break;
        }
        engagement_counter = engagement_counter + add;  // wrapping_add
        previous_state = state;
    }
    engagement_counter = decrement(engagement_counter);
    suppressed = engagement_counter > 151;
    cooldown = suppressed ? 90 : decrement(cooldown);
    disabled = (p.flags_2468 & 0x18000000u) != 0 || (p.flags_2472 & 0x8008u) != 0 ||
               (p.flags_2476 & 0x00400000u) != 0 || cooldown > 0 ||
               (p.category_2512 == 200 && (p.state_timer_2664 <= rs::from_bits(0x3da3d70au) || air_counter <= 10)) ||
               p.category_2512 == 500 || ((p.flags_2472 & 4u) != 0 && p.category_2512 != 400);
    elapsed = (p.category_2512 == 400 || state == 701) ? elapsed + p.timestep_2604 : 0.0f;
}

void GrindInputState::advance_history(const ProcessedPhysicsInput& p) {
    low_wheel_frames = (previous_proximity && p.state_2508 == 100 &&
                        static_cast<std::int32_t>(p.wheel_count_2556) < 2 && p.scalar_2652 < 0.8f)
                           ? low_wheel_frames + 1u
                           : 0u;
    grind_history = p.grind_words_2532_2536[1] == 2 ? 70u : decrement(grind_history);
    secondary_history = decrement(secondary_history);
    grounded_frames = p.category_2512 == 100 ? grounded_frames + 1u : 0u;
    air_frames = p.category_2512 == 100 ? 0u : air_frames + 1u;
}

MaterialMode GrindInputState::material_mode(const ProcessedPhysicsInput& p, bool nearby, bool targeting) {
    if (p.state_2508 == 300 || p.category_2512 == 500) {
        return MaterialMode::Unchanged;
    }
    const bool grind = (nearby && (p.state_2508 == 200 || p.state_2508 == 201)) || p.category_2512 == 400 ||
                       p.state_2508 == 701 || (p.category_2512 == 100 && previous_air_target);
    previous_air_target = p.state_2508 == 201 && targeting;
    return grind ? MaterialMode::Grind : MaterialMode::Standard;
}

namespace history {

std::uint32_t decrement(std::uint32_t value) {
    const std::uint32_t next = value - 1u;
    return (next & 0x80000000u) != 0 ? 0u : next;
}

}  // namespace history

}  // namespace s3::game::physics::player_input::grind
