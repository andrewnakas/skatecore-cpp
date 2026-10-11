// Port of skate-core/src/player/state_phase.rs (whole file).
// TU3 `PhysicalPlayerHiLOD::State` wrapper (`0x82DB6120`).
//
// State bodies, skateboard-controller modes, the board force queue, and the
// board's fixed-step cache are separate recovered systems. This module owns
// their exact wrapper order and Player+1344 accumulation.
#pragma once
#include <cstdint>

#include "s3/core/rs.hpp"

namespace s3::player::state_phase {

struct StatePhaseFields {
    /// PhysicalPlayer+1344.
    float elapsed_1344 = 0.0f;
    /// ProcessedPhysIn+2604.
    float timestep_2604 = 0.0f;
    /// SkateboardController+448.
    std::uint32_t controller_state_448 = 0;
    /// SkateboardController+452.
    bool controller_system_on_452 = false;
};
S3_DERIVE_EQ(StatePhaseFields, elapsed_1344, timestep_2604, controller_state_448, controller_system_on_452)

/// Calls made by `0x82DB6120`. Implementors must connect each method to the
/// corresponding recovered system; no fallback behavior is defined here.
struct StatePhaseServices {
    virtual ~StatePhaseServices() = default;
    /// Current-state vtable slot `+8`.
    virtual void update_current_state_vtable_8() = 0;
    virtual void update_controller_82d75f00() = 0;
    virtual void update_controller_mode_1_82d751d8() = 0;
    virtual void update_controller_mode_2_82d750f0() = 0;
    virtual void update_controller_mode_3_82d75bb8() = 0;
    virtual void update_controller_mode_4_82d75d58() = 0;
    /// First Player vtable `+116` lookup at `0x82DB61C4`.
    virtual void touch_skateboard_vtable_116() = 0;
    /// Second Player vtable `+116` lookup followed by `0x82C03718`.
    virtual void apply_skateboard_force_queue_82c03718() = 0;
    /// Third Player vtable `+116` lookup and the fixed-`1/60` VMX cache update
    /// at `0x82DB61F0..0x82DB62D0`.
    virtual void update_skateboard_fixed_step_cache_82db61f0() = 0;
};

inline void run_state_phase(StatePhaseFields& fields, StatePhaseServices& services) {
    services.update_current_state_vtable_8();
    if (fields.controller_system_on_452) {
        services.update_controller_82d75f00();
        switch (fields.controller_state_448) {
            case 1: services.update_controller_mode_1_82d751d8(); break;
            case 2: services.update_controller_mode_2_82d750f0(); break;
            case 3: services.update_controller_mode_3_82d75bb8(); break;
            case 4: services.update_controller_mode_4_82d75d58(); break;
            default: break;
        }
    }
    services.touch_skateboard_vtable_116();
    services.apply_skateboard_force_queue_82c03718();
    services.update_skateboard_fixed_step_cache_82db61f0();
    fields.elapsed_1344 += fields.timestep_2604;
}

}  // namespace s3::player::state_phase
