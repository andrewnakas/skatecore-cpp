// Port of skate-game/src/graph_host/motion_character_gesture.rs.
//! Original CharacterGesture82BAA320, instance82BAADD0 and helpers
//!82BAAFB0/82BAB080/82BAB258/82BAAED8. Allocation and channel names are owned
//! here; the original entry/cycle/exit decisions and channel timing are kept.
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>

#include "s3/core/graph/intents.hpp"
#include "s3/core/rs.hpp"
#include "s3/game/graph_host/motion_animation.hpp"

namespace s3::game::graph_host::motion_character_gesture {

using motion_animation::MotionAnimation;
using s3::graph::intents::IntentMap;

struct CharacterGestureInputs {
    /// Actor20 resolves IMotionGraph (8258F180); virtual16 returns this map.
    const IntentMap& motion_intents;
    /// Specific MotionGraph+C58/C5C. Nonzero counts make a hand busy.
    std::array<std::uint32_t, 2> busy_hands{};
    std::uint32_t filtered_state = 0;
    bool ground321 = false;
    bool board_held311 = false;
    bool state_offboard75 = false;
    /// PhysOutAnimation+72, used only while a gesture is active.
    float distance_to_cog = 0.0f;
    /// ISkaterAnim virtual56: Up, Down, Left, Right gesture catalog indices.
    std::optional<std::array<std::uint32_t, 4>> selections;
    /// ISkaterAnim virtual120, evaluated by82B988B8.
    bool suppress_up = false;
    /// Actor+44 virtual24. When true ForceBrake does not suppress a start.
    bool force_brake_bypass = false;
};

struct GesturePublication {
    ///8258FB30 writes specific MotionGraph+CE4 and sets byteCE8 to1.
    std::uint32_t gesture = 0;
    ///8258FB38 writes byteCE9 from direction==Down.
    bool down = false;
};

struct CharacterGesture {
    // Private in Rust.
    bool active = false;
    std::int32_t stage = -1;
    std::int32_t direction = -1;
    std::int32_t hands = -1;

    ///82BAAE70/7C/88/8C: no channel is owned until a real gesture starts.
    static CharacterGesture new_() { return CharacterGesture{}; }
    /// `impl Default` (delegates to `new`).
    static CharacterGesture default_() { return new_(); }

    /// EndGesture82BAA320's explicit teardown path. The native operation ends
    /// all gesture-owned channels in reverse owner order and clears the
    /// persistent selection state.
    void end(MotionAnimation& animation);

    rs::Result<std::optional<GesturePublication>, std::string> update(MotionAnimation& animation,
                                                                      const CharacterGestureInputs& input);

    // Private in Rust.
    rs::Result<rs::Unit, std::string> next_stage(MotionAnimation& animation, const CharacterGestureInputs& input,
                                                 std::int32_t stage);
    void clear_active();
};

//82F881D0..83C4 constructs this exact37-entry table at830BF5D8. (Private in Rust.)
extern const std::array<const char*, 37> NAMES;

}  // namespace s3::game::graph_host::motion_character_gesture
