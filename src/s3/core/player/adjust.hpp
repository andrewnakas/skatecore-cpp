// Port of skate-core/src/player/adjust.rs (whole file).
// TU3 `PhysicalPlayerHiLOD::Adjust` (`0x82DB6CF0`).
//
// This phase runs only after the solver completion fence. It publishes the
// solved board condition, performs the first skeleton adjustment, calls the
// current state's post-physics slot, then performs the final skeleton update.
#pragma once
#include <cstdint>

#include "s3/core/rs.hpp"

namespace s3::player::adjust {

struct AdjustFields {
    /// ProcessedPhysIn+2512.
    std::uint32_t processed_word_2512 = 0;
    /// ProcessedPhysIn+2604.
    float timestep_2604 = 0.0f;
    /// SkateboardController+448.
    std::uint32_t controller_state_448 = 0;
    /// Player+1876 output staging +42.
    bool output_flag_42 = false;
    /// Player+1876 output staging +144.
    float output_scalar_144 = 0.0f;
    /// Player+1876 output staging +200.
    std::uint32_t output_counter_200 = 0;
};
S3_DERIVE_EQ(AdjustFields, processed_word_2512, timestep_2604, controller_state_448, output_flag_42,
             output_scalar_144, output_counter_200)

/// Required object calls in the exact order used by `0x82DB6CF0`. (Rust trait.)
struct AdjustServices {
    virtual ~AdjustServices() = default;
    /// Player vtable `+116`, then SkateboardBody `0x82C02138`.
    virtual std::uint32_t board_condition_82c02138() = 0;
    /// BoardBody vtable `+4`, byte `+868`. Called only in controller mode 1.
    virtual bool controller_mode_one_board_flag_868() = 0;
    /// Skeleton update `0x82BD80D8` with the board flag and frame timestep.
    virtual void adjust_skeleton_82bd80d8(bool board_flag, float timestep) = 0;
    /// Current physical-state vtable slot `+40`.
    virtual void update_current_state_post_physics_vtable_40() = 0;
    /// Player helper `0x82DB9100`; its exact result is forwarded unchanged.
    virtual std::uint32_t player_output_selector_82db9100() = 0;
    /// Final skeleton update `0x82BD83E0`.
    virtual void finish_skeleton_adjustment_82bd83e0(std::uint32_t selector) = 0;
};

/// Runs the complete TU3 Adjust wrapper after solver completion.
inline void run_adjust(AdjustFields& fields, AdjustServices& services) {
    if (services.board_condition_82c02138() == 1 && fields.processed_word_2512 != 500) {
        fields.output_flag_42 = true;
        fields.output_scalar_144 = 0.0f;
        fields.output_counter_200 = fields.output_counter_200 + 1u;
    }
    const bool board_flag = fields.controller_state_448 == 1 ? services.controller_mode_one_board_flag_868() : false;
    services.adjust_skeleton_82bd80d8(board_flag, fields.timestep_2604);
    services.update_current_state_post_physics_vtable_40();
    const std::uint32_t selector = services.player_output_selector_82db9100();
    services.finish_skeleton_adjustment_82bd83e0(selector);
}

}  // namespace s3::player::adjust
