// Port of skate-game/src/physics/ground_runtime/entry.rs.
//! Actual Ground entry82D37538. Numerical operations touch live board bodies;
//! the physical skeleton/air modifier remain calls to their separate owners.
//! (`impl GroundState::enter` is declared in GroundState.)
#pragma once
#include <cstdint>
#include <functional>
#include <string>

#include "s3/core/physics/board_runtime.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::foot_ik {
struct FootIk;  // physics/foot_ik.hpp (group D)
}

namespace s3::game::physics::ground_runtime::entry {

/// `pub(super)`.
struct EntrySettings {
    float deck_angular_drag = 0.0f;
    float powerslide_exit = 0.0f;
    float landing_strength = 0.0f;
    float landing_offset = 0.0f;

    static rs::Result<EntrySettings, std::string> load(const sd::Collections& data);
};

///References to the actual lifecycle owners. These fields are not derived from
///controller input or cached separately in Ground. Collision and air-landing
///calls must apply the recovered physical owner behavior before returning.
struct GroundEntryTargets {
    foot_ik::FootIk& foot_ik;
    bool& skeleton_elapsed_16505;
    float& reckoning_spin_angle;
    float& reckoning_spin_speed;
    std::uint8_t& board_flags_8384;
    std::uint8_t& board_animated_290;
    std::uint32_t& wipeout_mode;
    float& wipeout_timer;
    /// Rust `&mut dyn FnMut(u32) -> Result<(), String>`.
    std::function<Status(std::uint32_t)> set_skeleton_collision_state;
    /// Rust `&mut dyn FnMut(bool) -> Result<(), String>`.
    std::function<Status(bool)> enter_air_landing_modifier;
};

/// Board-owned portion of SetStandard82C03AF8. 82C090D4/90EC write
/// assembly/part COLLISION groups, not rigid-body activation flags. Material
/// bindings and volume enables still require the live settings/volume owner.
/// (Private in Rust; exposed for entry_restoration_tests.)
void restore_standard_board_fields(sc::physics::board_runtime::BoardRuntime& board, std::uint8_t& flags,
                                   float drag);

}  // namespace s3::game::physics::ground_runtime::entry
