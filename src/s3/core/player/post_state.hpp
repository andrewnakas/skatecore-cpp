// Port of skate-core/src/player/post_state.rs (whole file).
// TU3 `PhysicalPlayerHiLOD::PostState` thunk (`0x82DB62F8`).
//
// Raw instructions load Player+1796 and tail-call the current physical
// state's vtable slot `+12`. The function has no other side effects.
#pragma once

namespace s3::player::post_state {

/// Mandatory current-state dispatch performed by `0x82DB62F8`.
struct PostStateServices {
    virtual ~PostStateServices() = default;
    virtual void update_current_state_vtable_12() = 0;
};

/// Runs the complete PhysicalPlayer PostState phase.
inline void run_post_state(PostStateServices& services) { services.update_current_state_vtable_12(); }

}  // namespace s3::player::post_state
