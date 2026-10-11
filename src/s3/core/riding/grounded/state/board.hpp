// Port of skate-core/src/riding/grounded/state/board.rs.
// Branch composition of `PhysicsGround::UpdateSkateboard` (`82D38800`).
#pragma once
#include <variant>

#include "s3/core/riding/grounded/drag.hpp"
#include "s3/core/riding/grounded/state/board_types.hpp"
#include "s3/core/riding/grounded/state/data.hpp"
#include "s3/core/riding/grounded/state/ordinary.hpp"
#include "s3/core/riding/speed_wobble.hpp"
#include "s3/core/riding/steering.hpp"

namespace s3::riding::grounded::state::board {

using board_types::GroundBoardComponents;
using board_types::GroundBoardError;
using board_types::GroundBoardInput;
using board_types::GroundBoardSettings;
using board_types::GroundBoardStage;
using data::PhysicsGroundState;

/// Rust `enum GroundBoardOutcome { Animated, Collision { tag_15_queued }, Ordinary(OrdinaryGroundResult) }`.
struct GroundBoardOutcome {
    struct Animated {};
    struct Collision {
        bool tag_15_queued = false;
    };
    struct Ordinary {
        ordinary::OrdinaryGroundResult _0;
    };
    std::variant<Animated, Collision, Ordinary> value;

    GroundBoardOutcome(Animated v) : value(v) {}
    GroundBoardOutcome(Collision v) : value(v) {}
    GroundBoardOutcome(Ordinary v) : value(v) {}
};
S3_DERIVE_EQ_EMPTY(GroundBoardOutcome::Animated)
S3_DERIVE_EQ(GroundBoardOutcome::Collision, tag_15_queued)
S3_DERIVE_EQ(GroundBoardOutcome::Ordinary, _0)
S3_DERIVE_EQ(GroundBoardOutcome, value)

namespace detail {

template <class S>
rs::Result<GroundBoardOutcome, GroundBoardError<typename S::BoardError>> animated(PhysicsGroundState& state,
                                                                                  GroundBoardComponents components,
                                                                                  S& services) {
    using Error = GroundBoardError<typename S::BoardError>;
    S3_GB_TRY_UNIT(
        service(GroundBoardStage::SetAnimatedVelocity, services.set_animated_velocity_82c04168(state.vector_2688)));
    S3_GB_TRY_UNIT(
        service(GroundBoardStage::WriteProcessedVelocity, services.write_processed_velocity_400(state.vector_2688)));
    {
        auto r = components.inertias.set_linear_drag(0.0f, drag::DragSelection::AllParts{});
        if (r.is_err()) return rs::make_err(Error(typename Error::Drag{r.unwrap_err()}));
    }
    S3_GB_TRY(const auto pose, service(GroundBoardStage::BuildAnimatedPose, services.build_animated_pose_82d33448()));
    S3_GB_TRY_UNIT(service(GroundBoardStage::PublishAnimatedPose, services.publish_animated_pose_82be33d0(pose)));
    S3_GB_TRY_UNIT(service(GroundBoardStage::UpdateExternalPlayer,
                           services.update_external_player_82d67848(pose, state.vector_2688)));
    S3_GB_TRY_UNIT(service(GroundBoardStage::CommitExternalPlayer, services.commit_external_player_82d68800()));
    S3_GB_TRY_UNIT(
        service(GroundBoardStage::FinalizeAnimatedBoard, services.finalize_animated_board_sk83_na_f_01a4()));
    state.flag_2720 = true;
    return GroundBoardOutcome(GroundBoardOutcome::Animated{});
}

}  // namespace detail

/// Runs the native steering/contact branch prefix, then exactly one of the
/// animated, collision-response, or ordinary grounded paths.
template <class S>
rs::Result<GroundBoardOutcome, GroundBoardError<typename S::BoardError>> update(PhysicsGroundState& state,
                                                                                GroundBoardComponents components,
                                                                                const GroundBoardSettings& settings,
                                                                                GroundBoardInput input, S& services) {
    const float steering_tilt = steering::calculate_tilt(settings.steering, input.steering,
                                                         &state.steering_push_scalar_2640,
                                                         &state.steering_damped_turn_2644);
    input.speed_wobble.tilt = steering_tilt;
    S3_GB_TRY(input.speed_wobble.center_of_mass_height,
              service(GroundBoardStage::CenterOfMassHeight, services.center_of_mass_height_82d38838()));
    const float tilt = speed_wobble::calculate(components.speed_wobble, settings.speed_wobble, input.speed_wobble);
    components.truck_steering.update(tilt, settings.steering.tilt_blending, input.truck_flags_2468,
                                     input.truck_flags_2472);

    S3_GB_TRY_UNIT(service(GroundBoardStage::SetWheelMaterials, services.set_contact_wheel_materials()));
    S3_GB_TRY(const board_types::GroundContactResponse contact,
              service(GroundBoardStage::ContactResponse,
                      services.contact_response_82d93df0(input.contact, state.vector_2688)));
    state.flag_2731 = contact.active_2731;
    state.vector_2688 = contact.vector_2688;
    state.scalar_2704 = contact.scalar_2704;
    state.flag_2708 = contact.animated_board_2708;
    state.flag_2720 = false;

    S3_GB_TRY_UNIT(service(GroundBoardStage::UpdateBodyAccumulator, services.update_body_accumulator_82d389dc()));
    if (state.flag_2708) {
        return detail::animated(state, components, services);
    }

    S3_GB_TRY(const std::optional<board_types::CollisionForceResponse> collision,
              service(GroundBoardStage::CollisionForce, services.collision_force_82d944e8(input.ground_vector_1216)));
    if (collision.has_value()) {
        state.collision_force_2528 = collision->force_2528;
        state.collision_point_2544 = collision->point_2544;
        state.vector_2592 = collision->vector_2592;
        state.collision_countdown_2652 = settings.collision_response_duration;
        S3_GB_TRY_UNIT(
            service(GroundBoardStage::ApplyCollisionVector, services.apply_vector_82c07000(collision->vector_2592)));
        components.speed_model.flags_1360 |= 0x80000000u;
        const bool queued = components.force_queue.append(collision->tagged_force());
        S3_GB_TRY(const float projection,
                  service(GroundBoardStage::CollisionProjection,
                          services.collision_force_dot_velocity_82d38e18(collision->force_2528,
                                                                         input.ground_force.velocity_400)));
        state.flag_2722 = projection < -0.75f;
        return GroundBoardOutcome(GroundBoardOutcome::Collision{queued});
    }

    S3_GB_TRY(const ordinary::OrdinaryGroundResult result,
              ordinary::update(state, components, settings, input, contact.tag_16_force, services));
    return GroundBoardOutcome(GroundBoardOutcome::Ordinary{result});
}

}  // namespace s3::riding::grounded::state::board
