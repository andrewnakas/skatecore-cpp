// Port of skate-core/src/player/offboard/landing_state.rs (whole file).
// State503, original TU3 82D4D418..82D4E100 (SHA431b8eba...0395a).
// Manager owns the trajectory; this state owns alignment and landing decisions.
#pragma once
#include <array>
#include <cstdint>
#include <optional>

#include "s3/core/player/offboard/landing_deck.hpp"

namespace s3::player::offboard::landing_state {

using Vector = std::array<float, 4>;
using landing_deck::Manager;
using landing_deck::UpdateOutput;

struct Settings {
    float minimum_auto_angle = 0.0f;
    float automatic_speed = 0.0f;
    float input_speed = 0.0f;
    float input_delta = 0.0f;
    float automatic_delta = 0.0f;
    float maximum_landing_speed = 0.0f;
};

struct Entry {
    std::uint32_t previous_category = 0;
    bool hippy = false;
    float strength = 0.0f;
    Vector board_position{};
    Vector com_position{};
    Vector com_velocity{};
    Vector board_velocity{};
    Vector up{};
    Vector hips_up{};
    Vector animation_right{};
    bool reversed = false;
};

struct State {
    std::optional<UpdateOutput> output;
    float time_to_land = 0.0f;
    bool dangerous = false;
    bool near_deck = false;
    bool turning = false;
    bool hippy = false;
    std::int32_t takeoff_frames = 0;
    float spin_rate = 0.0f;
    float applied_spin = 0.0f;
    float transition_angle = 0.0f;
    float accumulated_spin = 0.0f;
    std::int32_t half_turns = 0;
    std::int32_t next_half_turns = 0;

    ///82D4D418/530/668: same manager as BipedAir; preservation depends on category.
    void enter(Manager& manager, const Entry& input);
    ///82D4D838..DAFC, after the manager update and before Skeleton82BDE428.
    void align(const Settings& s, Vector board_forward, Vector animation_forward, float state_time,
               float com_velocity_y, float input_spin);
    ///82D4DB10..DB74, called only after the skeleton update completes.
    void advance_spin(const UpdateOutput& output);
    ///82D4DD80: actual physical toe-volume positions15/19, not IK targets.
    static float accurate_time(float board_y, float board_velocity_y, float com_velocity_y,
                               std::array<float, 2> toe_y, std::uint32_t wheel_contacts);
    ///82D4DCBC..DD58; leaves near_deck untouched while rising.
    void finish(const Settings& s, float com_velocity_y);
    std::int32_t landing_half_turns() const { return time_to_land >= 0.05f ? half_turns : next_half_turns; }
    bool requests_board_flip() const { return turning && time_to_land < 0.02f; }
};

}  // namespace s3::player::offboard::landing_state
