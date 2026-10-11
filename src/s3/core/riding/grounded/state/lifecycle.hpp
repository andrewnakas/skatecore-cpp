// Port of skate-core/src/riding/grounded/state/lifecycle.rs.
// Ground Enter `82D37538` and Exit `82D37B20` lifecycle order.
#pragma once
#include <array>
#include <cstdint>
#include <variant>

#include "s3/core/physics/force_queue.hpp"
#include "s3/core/physics/manual/state.hpp"
#include "s3/core/riding/grounded/drag.hpp"
#include "s3/core/riding/grounded/manual_entry.hpp"
#include "s3/core/riding/grounded/state/data.hpp"
#include "s3/core/riding/pumping/state.hpp"
#include "s3/core/riding/speed_wobble.hpp"

namespace s3::riding::grounded::state::lifecycle {

using data::PhysicsGroundState;
using drag::BodyInertias;
using drag::DragBindingError;
using drag::DragSelection;
using manual_entry::ManualGroundInput;
using physics::force_queue::BoardForceQueue;
using physics::force_queue::QueuedPointForce;
using physics::manual::state::ManualState;
using pumping::state::PumpingState;
using speed_wobble::SpeedWobbleState;

struct GroundEnterInput {
    std::uint32_t previous_category_2516 = 0;
    std::uint32_t previous_state_2504 = 0;
    float powerslide_exit_scale = 0.0f;
    ManualGroundInput manual_ground;
};

enum class GroundEnterStage {
    SetBoardStandard,
    InitializeSkeleton,
    SetCollisionState,
    ResetExternalObjects,
    InitializeBoardSpeed,
    EnterManual,
    EnterFromAir,
    BuildLandingOnDeckForce,
    SetControllerMode,
};

/// Rust `enum GroundEnterError<S, P> { Service { stage, source: S }, ManualProjection { stage, source: P } }`.
template <class SE, class PE>
struct GroundEnterError {
    struct Service {
        GroundEnterStage stage = GroundEnterStage::SetBoardStandard;
        SE source;
        bool operator==(const Service& o) const { return stage == o.stage && source == o.source; }
        bool operator!=(const Service& o) const { return !(*this == o); }
    };
    struct ManualProjection {
        GroundEnterStage stage = GroundEnterStage::SetBoardStandard;
        PE source;
        bool operator==(const ManualProjection& o) const { return stage == o.stage && source == o.source; }
        bool operator!=(const ManualProjection& o) const { return !(*this == o); }
    };
    std::variant<Service, ManualProjection> value;

    GroundEnterError(Service v) : value(std::move(v)) {}
    GroundEnterError(ManualProjection v) : value(std::move(v)) {}
    bool operator==(const GroundEnterError& o) const { return value == o.value; }
    bool operator!=(const GroundEnterError& o) const { return !(*this == o); }
};

struct GroundEnterResult {
    bool landing_force_attempted = false;
    bool landing_force_queued = false;
};
S3_DERIVE_EQ(GroundEnterResult, landing_force_attempted, landing_force_queued)

/// Rust trait `GroundEnterServices` (duck-typed template parameter `S`).
/// Required object operations surrounding the state-owned resets. None have a
/// default because their complete side effects are part of Ground entry.
///   using Error = ...;
///   rs::Result<rs::Unit, Error> set_board_standard_82c03af8();
///   rs::Result<rs::Unit, Error> initialize_ground_skeleton();
///   rs::Result<rs::Unit, Error> set_collision_state_82d911a8(std::uint32_t state);
///   rs::Result<rs::Unit, Error> reset_ground_external_objects();
///   /// Full `82D375D0..82D376CC` board transform/velocity projection and target
///   /// speed initialization, preserving native VMX arithmetic.
///   rs::Result<rs::Unit, Error> initialize_board_speed_82d375d0();
///   rs::Result<rs::Unit, Error> enter_from_air_82bf2f18();
///   /// Full tag-19 producer `82D377AC..82D378A8`.
///   rs::Result<QueuedPointForce, Error> landing_on_deck_force();
///   rs::Result<rs::Unit, Error> set_controller_mode_one();

/// Rust `&'a mut` borrows are held as references.
struct GroundEntryComponents {
    PumpingState& pumping;
    SpeedWobbleState& speed_wobble;
    ManualState& manual;
    BoardForceQueue& force_queue;
};

/// Runs every verified Ground entry operation in native order. The service
/// boundaries are unresolved engine/numerical functions, not optional hooks.
/// `B`: ManualGroundBodies, `P`: ManualGroundProjection (see manual_entry.hpp).
template <class S, class B, class P>
rs::Result<GroundEnterResult, GroundEnterError<typename S::Error, typename P::Error>> enter(
    PhysicsGroundState& state, GroundEntryComponents components, GroundEnterInput input, B& bodies, P& projection,
    S& services) {
    using Error = GroundEnterError<typename S::Error, typename P::Error>;
#define S3_ENTER_CALL(stage_, expr)                                                                   \
    do {                                                                                              \
        auto s3_enter_r_ = (expr);                                                                    \
        if (s3_enter_r_.is_err())                                                                     \
            return rs::make_err(Error(typename Error::Service{stage_, s3_enter_r_.unwrap_err()}));   \
    } while (0)
    S3_ENTER_CALL(GroundEnterStage::SetBoardStandard, services.set_board_standard_82c03af8());
    S3_ENTER_CALL(GroundEnterStage::InitializeSkeleton, services.initialize_ground_skeleton());
    S3_ENTER_CALL(GroundEnterStage::SetCollisionState, services.set_collision_state_82d911a8(6));

    state.word_2560 = 0;
    state.word_2564 = 0;
    state.flag_2708 = false;
    state.controls_latched_2725 = false;
    state.captured_position_valid_2726 = false;
    state.was_pinning_2728 = false;
    state.flag_2731 = false;
    state.manual_correction_2732 = false;
    state.manual_opposition_2733 = false;

    S3_ENTER_CALL(GroundEnterStage::ResetExternalObjects, services.reset_ground_external_objects());
    S3_ENTER_CALL(GroundEnterStage::InitializeBoardSpeed, services.initialize_board_speed_82d375d0());
    components.speed_wobble.reset();
    {
        auto r = manual_entry::enter_ground(components.manual, input.previous_category_2516,
                                            input.powerslide_exit_scale, input.manual_ground, bodies, projection);
        if (r.is_err()) {
            return rs::make_err(Error(typename Error::ManualProjection{GroundEnterStage::EnterManual, r.unwrap_err()}));
        }
    }

    if (input.previous_category_2516 == 200) {
        S3_ENTER_CALL(GroundEnterStage::EnterFromAir, services.enter_from_air_82bf2f18());
    }
    GroundEnterResult result{false, false};
    if (input.previous_state_2504 == 503) {
        auto r = services.landing_on_deck_force();
        if (r.is_err()) {
            return rs::make_err(Error(typename Error::Service{GroundEnterStage::BuildLandingOnDeckForce, r.unwrap_err()}));
        }
        QueuedPointForce force = r.unwrap();
        force.tag = 19;
        result.landing_force_attempted = true;
        result.landing_force_queued = components.force_queue.append(force);
    }

    state.straighten_scale_2672 = input.previous_state_2504 == 101 ? rs::from_bits(0x3e23d70au) : 1.0f;
    components.pumping.reset();
    S3_ENTER_CALL(GroundEnterStage::SetControllerMode, services.set_controller_mode_one());
#undef S3_ENTER_CALL

    state.steering_push_scalar_2640 = 1.0f;
    state.steering_damped_turn_2644 = 0.0f;
    state.elapsed_2648 = 0.0f;
    state.collision_countdown_2652 = 0.0f;
    state.vector_2592 = {0.0f, 0.0f, 0.0f, 0.0f};
    state.vector_2608 = {0.0f, 0.0f, 0.0f, 0.0f};
    state.anti_flip_torque_2624 = {0.0f, 0.0f, 0.0f, 0.0f};
    state.hang_detection_frames_2740 = 0;
    state.hang_force_frames_2744 = 0;
    state.hung_wipeout_frames_2748 = 0;
    state.anti_flip_nudge_frames_2752 = 0;
    state.flag_2721 = false;
    state.anti_flip_nudge_applied_2723 = false;
    state.push_suppressed_2730 = false;
    return result;
}

enum class GroundExitStage {
    SetWheelMaterials,
    RestoreAnimatedBoardVelocity,
};

/// Rust `enum GroundExitError<E> { Drag(DragBindingError), Service { stage, source: E } }`.
template <class E>
struct GroundExitError {
    struct Drag {
        DragBindingError _0 = DragBindingError::PartOutsideAssembly;
        bool operator==(const Drag& o) const { return _0 == o._0; }
        bool operator!=(const Drag& o) const { return !(*this == o); }
    };
    struct Service {
        GroundExitStage stage = GroundExitStage::SetWheelMaterials;
        E source;
        bool operator==(const Service& o) const { return stage == o.stage && source == o.source; }
        bool operator!=(const Service& o) const { return !(*this == o); }
    };
    std::variant<Drag, Service> value;

    GroundExitError(Drag v) : value(std::move(v)) {}
    GroundExitError(Service v) : value(std::move(v)) {}
    bool operator==(const GroundExitError& o) const { return value == o.value; }
    bool operator!=(const GroundExitError& o) const { return !(*this == o); }
};

/// Rust trait `GroundExitServices` (duck-typed template parameter `S`):
///   using Error = ...;
///   rs::Result<rs::Unit, Error> set_default_wheel_materials();
///   /// Full `82D37BAC..82D37C30` jump-velocity clamp and board velocity write.
///   rs::Result<rs::Unit, Error> restore_animated_board_velocity(std::array<float,4> vector_2688);

template <class S>
rs::Result<rs::Unit, GroundExitError<typename S::Error>> exit(PhysicsGroundState& state, PumpingState& pumping,
                                                              BodyInertias& inertias, S& services) {
    using Error = GroundExitError<typename S::Error>;
    state.steering_damped_turn_2644 = 0.0f;
    state.steering_push_scalar_2640 = 1.0f;
    pumping.reset();
    {
        auto r = inertias.set_linear_drag(0.0f, DragSelection::AllParts{});
        if (r.is_err()) return rs::make_err(Error(typename Error::Drag{r.unwrap_err()}));
    }
    {
        auto r = services.set_default_wheel_materials();
        if (r.is_err()) {
            return rs::make_err(Error(typename Error::Service{GroundExitStage::SetWheelMaterials, r.unwrap_err()}));
        }
    }
    if (state.flag_2708) {
        auto r = services.restore_animated_board_velocity(state.vector_2688);
        if (r.is_err()) {
            return rs::make_err(
                Error(typename Error::Service{GroundExitStage::RestoreAnimatedBoardVelocity, r.unwrap_err()}));
        }
    }
    return rs::Unit{};
}

}  // namespace s3::riding::grounded::state::lifecycle
