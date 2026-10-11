// Port of skate-core/src/player/lifecycle.rs (whole file).
// Physical-player state changes recovered from TU3 `0x82DB8540`.
//
// The selected state objects remain owned by `PhysicalPlayer`; changing state
// selects one of those objects, rather than constructing a replacement.
// Rust traits (SkateboardControllerActions, PhysicalStateCalls) -> abstract interfaces.
#pragma once
#include <cstdint>

#include "s3/core/player/state.hpp"
#include "s3/core/rs.hpp"

namespace s3::player::lifecycle {

using state::PhysicalStateId;
using state::UnknownPhysicalState;

/// The state object selected from one of `PhysicalPlayer`'s owned pointers.
struct StateBinding {
    PhysicalStateId state = PhysicalStateId::Sleeping;
    std::uint32_t owner_offset = 0;

    static constexpr StateBinding new_(PhysicalStateId state) {
        return StateBinding{state, state::native_owner_offset(state)};
    }
};
S3_DERIVE_EQ(StateBinding, state, owner_offset)

/// State-change fields owned directly by `PhysicalPlayer`.
struct PlayerStateChangeFields {
    /// Player+1312. `SetPhysicsState` clears it before publishing state history.
    std::uint32_t word_1312 = 0;
    /// Player+1336. Updated to the previous category only across a category edge.
    std::uint32_t previous_category_latch_1336 = 0;
    /// Player+1344. `SetPhysicsState` clears it on every accepted transition.
    float scalar_1344 = 0.0f;
};
S3_DERIVE_EQ(PlayerStateChangeFields, word_1312, previous_category_latch_1336, scalar_1344)

/// The exact `ProcessedPhysIn` fields read or written by `0x82DB8540`.
struct ProcessedStateChangeFields {
    /// +2480; bits 7 and 8 choose free-board controller mode on off-board entry.
    std::uint32_t flags_2480 = 0;
    /// +2500; requested state.
    std::uint32_t requested_state_2500 = 0;
    /// +2504; type reported by the old state's vtable slot +28.
    std::uint32_t previous_state_2504 = 0;
    /// +2508; current concrete state published before old-state Exit.
    std::uint32_t current_state_2508 = 0;
    /// +2512; category of `current_state_2508`.
    std::uint32_t current_category_2512 = 0;
    /// +2516; category of `previous_state_2504`.
    std::uint32_t previous_category_2516 = 0;
    /// +2520; copy of Player+1336 after its conditional update.
    std::uint32_t previous_category_latch_2520 = 0;
    /// +2564; reset before the old state's Exit call.
    std::uint32_t word_2564 = 0;
    /// +2664; reset before the old state's Exit call.
    float scalar_2664 = 0.0f;
};
S3_DERIVE_EQ(ProcessedStateChangeFields, flags_2480, requested_state_2500, previous_state_2504, current_state_2508,
             current_category_2512, previous_category_2516, previous_category_latch_2520, word_2564, scalar_2664)

/// Fields at `PhysicalPlayer+1844`'s `SkateboardController`.
struct SkateboardControllerFields {
    /// Controller+444. Its wider meaning is unresolved; every mode change here
    /// resets it with a 32-bit zero.
    std::uint32_t word_444 = 0;
    /// Controller+448. Values used by this path are stopped=0, held=1, free=2.
    std::uint32_t state_448 = 0;
    /// Controller+452, identified by `StopController` as the system-on byte.
    bool system_on_452 = false;
};
S3_DERIVE_EQ(SkateboardControllerFields, word_444, state_448, system_on_452)

/// Complete mutable data touched by the state-change function itself.
struct StateChangeData {
    PlayerStateChangeFields player;
    ProcessedStateChangeFields processed;
    SkateboardControllerFields skateboard_controller;
};
S3_DERIVE_EQ(StateChangeData, player, processed, skateboard_controller)

/// Board and drive operations called by the controller branch. Their complete
/// implementations are `HoldSkateboard` `0x82D75370` and
/// `LetGoOfSkateboard` `0x82D75440`; this lifecycle never replaces them.
struct SkateboardControllerActions {
    virtual ~SkateboardControllerActions() = default;
    virtual void hold_skateboard() = 0;
    virtual void let_go_of_skateboard() = 0;
};

/// Context supplied to an owned state's Enter or Exit implementation.
/// `active` makes the pointer-switch boundary observable without exposing a
/// native pointer in engine-independent code.
struct StateCall {
    StateBinding state;
    StateBinding active;
};
S3_DERIVE_EQ(StateCall, state, active)

/// Calls corresponding to the recovered state vtable slots used here.
struct PhysicalStateCalls {
    virtual ~PhysicalStateCalls() = default;
    /// Vtable +28 on the currently selected state.
    virtual PhysicalStateId get_type(StateBinding state) = 0;
    /// Vtable +20 on the old state.
    virtual void exit(StateCall call) = 0;
    /// Vtable +16 on the newly selected state.
    virtual void enter(StateCall call) = 0;
};

namespace detail {
/// `StopController` `0x82D75EA0`, called directly by `SetPhysicsState` for
/// every requested state outside 500..=502.
inline void stop_skateboard_controller(SkateboardControllerFields& controller, SkateboardControllerActions& actions) {
    if (!controller.system_on_452) {
        return;
    }
    const std::uint32_t previous_state = controller.state_448;
    controller.word_444 = 0;
    if (previous_state != 0) {
        actions.let_go_of_skateboard();
        controller.state_448 = 0;
    }
    controller.system_on_452 = false;
}

inline void prepare_skateboard_controller(PhysicalStateId requested, SkateboardControllerFields& controller,
                                          std::uint32_t processed_flags_2480, SkateboardControllerActions& actions) {
    if (!(requested == PhysicalStateId::BipedGround || requested == PhysicalStateId::BipedAir ||
          requested == PhysicalStateId::OffBoardPushing)) {
        stop_skateboard_controller(controller, actions);
        return;
    }
    if (controller.system_on_452) {
        return;
    }
    controller.word_444 = 0;
    if ((processed_flags_2480 & (0x80u | 0x100u)) != 0) {
        if (controller.state_448 != 2) {
            actions.let_go_of_skateboard();
            controller.state_448 = 2;
        }
    } else if (controller.state_448 != 1) {
        actions.hold_skateboard();
        controller.state_448 = 1;
    }
    controller.system_on_452 = true;
}
}  // namespace detail

class PhysicalPlayerStateLifecycle {
public:
    static constexpr PhysicalPlayerStateLifecycle new_(PhysicalStateId initial_state) {
        return PhysicalPlayerStateLifecycle(StateBinding::new_(initial_state));
    }

    constexpr StateBinding active() const { return active_; }

    /// Port of `PhysicalPlayerHiLOD::SetPhysicsState` `0x82DB8540`.
    ///
    /// Unknown numeric IDs are rejected before any callbacks or field writes.
    /// The native default branches exit and re-enter the existing pointer for
    /// such values, but no recovered caller contract establishes them as valid
    /// states; making that case an error prevents a silent false transition.
    rs::Result<StateBinding, UnknownPhysicalState> set_physics_state(std::uint32_t requested_raw,
                                                                     StateChangeData& data,
                                                                     PhysicalStateCalls& states,
                                                                     SkateboardControllerActions& controller_actions) {
        const auto parsed = state::try_from(requested_raw);
        if (parsed.is_err()) {
            return rs::make_err(parsed.unwrap_err());
        }
        const PhysicalStateId requested = parsed.unwrap();

        detail::prepare_skateboard_controller(requested, data.skateboard_controller, data.processed.flags_2480,
                                              controller_actions);

        data.player.word_1312 = 0;
        data.player.scalar_1344 = 0.0f;
        data.processed.scalar_2664 = 0.0f;
        data.processed.word_2564 = 0;

        const PhysicalStateId previous = states.get_type(active_);
        const std::uint32_t previous_category = state::category(previous);
        const std::uint32_t current_category = state::category(requested);
        data.processed.previous_state_2504 = static_cast<std::uint32_t>(previous);
        data.processed.previous_category_2516 = previous_category;
        data.processed.requested_state_2500 = static_cast<std::uint32_t>(requested);
        data.processed.current_state_2508 = static_cast<std::uint32_t>(requested);
        data.processed.current_category_2512 = current_category;
        if (current_category != previous_category) {
            data.player.previous_category_latch_1336 = previous_category;
        }
        data.processed.previous_category_latch_2520 = data.player.previous_category_latch_1336;

        states.exit(StateCall{active_, active_});
        active_ = StateBinding::new_(requested);
        states.enter(StateCall{active_, active_});
        return active_;
    }

    bool operator==(const PhysicalPlayerStateLifecycle& o) const { return active_ == o.active_; }
    bool operator!=(const PhysicalPlayerStateLifecycle& o) const { return !(*this == o); }

private:
    constexpr explicit PhysicalPlayerStateLifecycle(StateBinding active) : active_(active) {}
    StateBinding active_;
};

}  // namespace s3::player::lifecycle
