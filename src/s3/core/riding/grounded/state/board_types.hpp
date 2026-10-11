// Port of skate-core/src/riding/grounded/state/board_types.rs.
// Typed boundaries for `PhysicsGround::UpdateSkateboard` (`82D38800`).
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <variant>

#include "s3/core/math.hpp"
#include "s3/core/physics/force_queue.hpp"
#include "s3/core/physics/manual/controller.hpp"
#include "s3/core/physics/manual/settings.hpp"
#include "s3/core/physics/manual/state.hpp"
#include "s3/core/riding/anti_flip.hpp"
#include "s3/core/riding/braking.hpp"
#include "s3/core/riding/ground_force.hpp"
#include "s3/core/riding/grounded/drag.hpp"
#include "s3/core/riding/grounded/propulsion.hpp"
#include "s3/core/riding/grounded/state/corrections.hpp"
#include "s3/core/riding/heading.hpp"
#include "s3/core/riding/pumping.hpp"
#include "s3/core/riding/slide_friction.hpp"
#include "s3/core/riding/speed_model.hpp"
#include "s3/core/riding/speed_wobble.hpp"
#include "s3/core/riding/steering.hpp"
#include "s3/core/riding/straighten.hpp"

namespace s3::riding::grounded::state::board_types {

using anti_flip::AntiFlipInput;
using anti_flip::AntiFlipSettings;
using braking::LinearDragSettings;
using corrections::AntiFlipNudgeInput;
using corrections::HalfpipeWheelCatchInput;
using corrections::HangUpInput;
using corrections::PinningInput;
using drag::BodyInertias;
using drag::GroundDragInput;
using ground_force::GroundForceSettings;
using heading::HeadingInput;
using heading::HeadingSettings;
using math::Vector3;
using physics::force_queue::BoardForceQueue;
using physics::force_queue::QueuedPointForce;
using physics::manual::controller::ManualInput;
using physics::manual::settings::ManualMode;
using physics::manual::settings::ManualSettings;
using physics::manual::state::ManualState;
using propulsion::GroundPropulsionInput;
using propulsion::GroundPropulsionSettings;
using pumping::PumpForceInput;
using slide_friction::SlideFrictionInput;
using slide_friction::SlideFrictionSettings;
using speed_model::SpeedModelInput;
using speed_model::SpeedModelSettings;
using speed_model::SpeedModelState;
using speed_wobble::SpeedWobbleInput;
using speed_wobble::SpeedWobbleSettings;
using speed_wobble::SpeedWobbleState;
using steering::SteeringInput;
using steering::SteeringSettings;
using steering::TruckSteeringState;
using straighten::StraightenInput;
using straighten::StraightenSettings;

/// Rust borrows (`&'a T`) are held as const references.
struct GroundBoardSettings {
    const SteeringSettings& steering;
    const SpeedWobbleSettings& speed_wobble;
    GroundPropulsionSettings propulsion;
    const GroundForceSettings& ground_force;
    const SpeedModelSettings& speed_model;
    const SlideFrictionSettings& slide_friction;
    const StraightenSettings& straighten;
    const HeadingSettings& heading;
    const AntiFlipSettings& anti_flip;
    const ManualSettings& manual;
    ManualMode manual_mode;
    LinearDragSettings linear_drag;
    /// Global selected collision layout +172.
    float collision_response_duration = 0.0f;
    /// Global selected collision layout +176.
    float collision_response_scale = 0.0f;
    /// Global selected ground layout +272.
    float ground_force_contact_time_limit = 0.0f;
    /// Live scalar `flt_82098D0C`.
    float speed_model_reset_force_squared = 0.0f;
};

/// Values used to construct both `82D931E8` calls in their native caller.
struct GroundForceFrame {
    float argument_1_2752 = 0.0f;
    float processed_2776 = 0.0f;
    float processed_2780 = 0.0f;
    float previous_state_scalar_56 = 0.0f;
    float ground_scalar_1216 = 0.0f;
    float ground_scalar_1232 = 0.0f;
    float ground_scalar_1236 = 0.0f;
    float ground_scalar_1240 = 0.0f;
    float balance_2720 = 0.0f;
    float surface_speed_2656 = 0.0f;
    std::array<float, 4> axis_384{};
    std::array<float, 4> velocity_400{};
    std::array<float, 4> axis_544{};
};

/// Contiguous local record copied from board-body +8032 through +8084 before
/// the `82D93DF0` call. The names deliberately retain offsets where semantics
/// have not been independently recovered.
struct GroundContactFrame {
    std::array<float, 4> vector_8032{};
    std::array<float, 4> vector_8048{};
    std::array<float, 4> vector_8064{};
    std::uint32_t word_8080 = 0;
    bool flag_8084 = false;
    float scalar_2752 = 0.0f;
    float scalar_2756 = 0.0f;
};
S3_DERIVE_EQ(GroundContactFrame, vector_8032, vector_8048, vector_8064, word_8080, flag_8084, scalar_2752,
             scalar_2756)

struct GroundBoardInput {
    SteeringInput steering;
    SpeedWobbleInput speed_wobble;
    std::uint32_t truck_flags_2468 = 0;
    std::uint32_t truck_flags_2472 = 0;
    GroundContactFrame contact;
    /// Ground context +1216, copied verbatim for `82D944E8`.
    std::array<float, 4> ground_vector_1216{};
    GroundPropulsionInput propulsion;
    GroundForceFrame ground_force;
    SpeedModelInput speed_model;
    SlideFrictionInput slide_friction;
    PumpForceInput pump_force;
    StraightenInput straighten;
    HeadingInput heading;
    AntiFlipInput anti_flip;
    ManualInput manual;
    GroundDragInput ground_drag;
    float contact_time_2756 = 0.0f;
    std::uint32_t trajectory_state_1776_bits = 0;
    AntiFlipNudgeInput anti_flip_nudge;
    HangUpInput hang_up;
    HalfpipeWheelCatchInput halfpipe_wheel_catch;
    PinningInput pinning;
};

/// Rust `&'a mut` borrows are held as references.
struct GroundBoardComponents {
    SpeedWobbleState& speed_wobble;
    TruckSteeringState& truck_steering;
    SpeedModelState& speed_model;
    ManualState& manual;
    /// Board+284, separate from the five-field manual controller state.
    float& heading_previous;
    BoardForceQueue& force_queue;
    BodyInertias& inertias;
};

struct GroundContactResponse {
    bool active_2731 = false;
    QueuedPointForce tag_16_force;
    std::array<float, 4> vector_2688{};
    float scalar_2704 = 0.0f;
    bool animated_board_2708 = false;
};
S3_DERIVE_EQ(GroundContactResponse, active_2731, tag_16_force, vector_2688, scalar_2704, animated_board_2708)

namespace detail {
inline Vector3 xyz(const std::array<float, 4>& value) { return Vector3{value[0], value[1], value[2]}; }
}  // namespace detail

struct CollisionForceResponse {
    std::array<float, 4> force_2528{};
    std::array<float, 4> point_2544{};
    std::array<float, 4> vector_2592{};

    QueuedPointForce tagged_force() const {
        return QueuedPointForce{15, detail::xyz(force_2528), detail::xyz(point_2544)};
    }
};
S3_DERIVE_EQ(CollisionForceResponse, force_2528, point_2544, vector_2592)

enum class GroundBoardStage {
    CenterOfMassHeight,
    SetWheelMaterials,
    ContactResponse,
    UpdateBodyAccumulator,
    SetAnimatedVelocity,
    WriteProcessedVelocity,
    BuildAnimatedPose,
    PublishAnimatedPose,
    UpdateExternalPlayer,
    CommitExternalPlayer,
    FinalizeAnimatedBoard,
    CollisionForce,
    ApplyCollisionVector,
    CollisionProjection,
    ManualEffect,
    PushForceSquared,
    AntiFlipNudge,
    ApplyCollisionDecay,
    ApplyStraighten,
    ApplyHeading,
    ApplyAntiFlip,
    ApplyManual,
    HangUps,
    HalfpipeWheelCatch,
    Pinning,
    StrongForceSquared,
};

/// Rust `enum GroundBoardError<E> { Service { stage, source }, Manual(ManualError<E>),
/// Drag(DragBindingError) }`.
template <class E>
struct GroundBoardError {
    struct Service {
        GroundBoardStage stage = GroundBoardStage::CenterOfMassHeight;
        E source;
        bool operator==(const Service& o) const { return stage == o.stage && source == o.source; }
        bool operator!=(const Service& o) const { return !(*this == o); }
    };
    struct Manual {
        physics::manual::controller::ManualError<E> _0;
        bool operator==(const Manual& o) const { return _0 == o._0; }
        bool operator!=(const Manual& o) const { return !(*this == o); }
    };
    struct Drag {
        drag::DragBindingError _0 = drag::DragBindingError::PartOutsideAssembly;
        bool operator==(const Drag& o) const { return _0 == o._0; }
        bool operator!=(const Drag& o) const { return !(*this == o); }
    };
    std::variant<Service, Manual, Drag> value;

    GroundBoardError(Service v) : value(std::move(v)) {}
    GroundBoardError(Manual v) : value(std::move(v)) {}
    GroundBoardError(Drag v) : value(std::move(v)) {}
    bool operator==(const GroundBoardError& o) const { return value == o.value; }
    bool operator!=(const GroundBoardError& o) const { return !(*this == o); }
};

/// Rust trait `GroundBoardServices` (duck-typed template parameter `S`).
/// Engine/numerical operations whose bodies do not yet exist in readable core
/// code. Every operation is mandatory on the branch where native code calls it.
/// Supertraits: ManualAngleMeasurement + AntiFlipNudgeMath + HangUpServices +
/// HalfpipeWheelCatchServices + PinningServices, all with `Error = BoardError`
/// (a C++ implementor defines `using Error = BoardError;` once).
///   using BoardError = ...;
///   using AnimatedPose = ...;
///   rs::Result<float, BoardError> center_of_mass_height_82d38838();
///   rs::Result<rs::Unit, BoardError> set_contact_wheel_materials();
///   rs::Result<GroundContactResponse, BoardError> contact_response_82d93df0(
///       GroundContactFrame frame, std::array<float,4> previous_vector_2688);
///   rs::Result<rs::Unit, BoardError> update_body_accumulator_82d389dc();
///   rs::Result<rs::Unit, BoardError> set_animated_velocity_82c04168(std::array<float,4> velocity);
///   rs::Result<rs::Unit, BoardError> write_processed_velocity_400(std::array<float,4> velocity);
///   rs::Result<AnimatedPose, BoardError> build_animated_pose_82d33448();
///   rs::Result<rs::Unit, BoardError> publish_animated_pose_82be33d0(const AnimatedPose& pose);
///   rs::Result<rs::Unit, BoardError> update_external_player_82d67848(const AnimatedPose& pose,
///                                                                    std::array<float,4> velocity);
///   rs::Result<rs::Unit, BoardError> commit_external_player_82d68800();
///   rs::Result<rs::Unit, BoardError> finalize_animated_board_sk83_na_f_01a4();
///   rs::Result<std::optional<CollisionForceResponse>, BoardError> collision_force_82d944e8(
///       std::array<float,4> ground_vector_1216);
///   /// Exact normalize/threshold/dot sequence at `82D38E18..82D38EA8`.
///   rs::Result<float, BoardError> collision_force_dot_velocity_82d38e18(std::array<float,4> force,
///                                                                       std::array<float,4> velocity_400);
///   rs::Result<rs::Unit, BoardError> apply_vector_82c07000(std::array<float,4> vector);
///   rs::Result<rs::Unit, BoardError> apply_angular_displacement_82c075b8(std::array<float,4> vector);

}  // namespace s3::riding::grounded::state::board_types
