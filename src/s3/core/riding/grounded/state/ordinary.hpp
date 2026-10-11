// Port of skate-core/src/riding/grounded/state/ordinary.rs (module-private in Rust).
// Ordinary (non-animated, non-collision-response) `82D38800` path.
#pragma once
#include <array>
#include <cstdint>
#include <utility>
#include <variant>

#include "s3/core/math.hpp"
#include "s3/core/physics/force_queue.hpp"
#include "s3/core/physics/manual/controller.hpp"
#include "s3/core/riding/anti_flip.hpp"
#include "s3/core/riding/ground_force.hpp"
#include "s3/core/riding/grounded/propulsion.hpp"
#include "s3/core/riding/grounded/state/board_types.hpp"
#include "s3/core/riding/grounded/state/corrections.hpp"
#include "s3/core/riding/grounded/state/data.hpp"
#include "s3/core/riding/heading.hpp"
#include "s3/core/riding/pumping.hpp"
#include "s3/core/riding/slide_friction.hpp"
#include "s3/core/riding/speed_model.hpp"
#include "s3/core/riding/straighten.hpp"

namespace s3::riding::grounded::state::board {

/// board.rs `pub(super) fn service`; defined here because ordinary.rs and
/// board.rs reference each other.
template <class T, class E>
rs::Result<T, board_types::GroundBoardError<E>> service(board_types::GroundBoardStage stage, rs::Result<T, E> result) {
    using Error = board_types::GroundBoardError<E>;
    if (result.is_err()) {
        return rs::make_err(Error(typename Error::Service{stage, result.unwrap_err()}));
    }
    return result.unwrap();
}

}  // namespace s3::riding::grounded::state::board

// `?` on an rs::Result whose error type already matches the function's.
#define S3_GB_TRY(decl, expr)                                         \
    auto S3_PP_CAT(s3_gb_r_, __LINE__) = (expr);                      \
    if (S3_PP_CAT(s3_gb_r_, __LINE__).is_err())                       \
        return rs::make_err(S3_PP_CAT(s3_gb_r_, __LINE__).unwrap_err()); \
    decl = S3_PP_CAT(s3_gb_r_, __LINE__).unwrap()
#define S3_GB_TRY_UNIT(expr)                                          \
    do {                                                              \
        auto s3_gb_u_ = (expr);                                       \
        if (s3_gb_u_.is_err()) return rs::make_err(s3_gb_u_.unwrap_err()); \
    } while (0)

namespace s3::riding::grounded::state::ordinary {

using board_types::GroundBoardComponents;
using board_types::GroundBoardError;
using board_types::GroundBoardInput;
using board_types::GroundBoardSettings;
using board_types::GroundBoardStage;
using board_types::GroundForceFrame;
using data::PhysicsGroundState;
using math::Vector3;
using physics::force_queue::QueuedPointForce;
using propulsion::PropulsionSubmission;

struct OrdinaryGroundResult {
    bool manual_correction = false;
    std::uint32_t terminal_force_tag = 0;
    bool terminal_force_queued = false;
    bool speed_model_reset = false;
};
S3_DERIVE_EQ(OrdinaryGroundResult, manual_correction, terminal_force_tag, terminal_force_queued, speed_model_reset)

namespace detail {

inline float first_balance_selector(float value) {
    if (value <= 0.0f) {
        return value >= -0.0f ? 1.0f : 2.0f;
    }
    return 0.0f;
}

inline float second_balance_selector(float value) {
    const float nonnegative = value <= 0.0f ? 1.0f : 2.0f;
    return value >= -0.0f ? nonnegative : 0.0f;
}

inline QueuedPointForce force_record(std::uint32_t tag, const std::array<float, 8>& value) {
    return QueuedPointForce{tag, Vector3{value[0], value[1], value[2]}, Vector3{value[4], value[5], value[6]}};
}

inline std::array<float, 4> vector4(Vector3 value) { return {value.x, value.y, value.z, 0.0f}; }

inline std::pair<std::array<float, 8>, std::array<float, 8>> ground_forces(
    const ground_force::GroundForceSettings& settings, const GroundForceFrame& frame) {
    float side_scalar = 0.0f;
    if (frame.processed_2776 == 0.0f || frame.processed_2780 < 0.0f) {
        const float value = frame.ground_scalar_1232 * frame.previous_state_scalar_56;
        side_scalar = -value >= 0.0f ? 0.0f : value;
    }
    const auto common = [&](float application_z, float argument_3, float argument_4) {
        ground_force::GroundForceInput in;
        in.argument_1 = frame.argument_1_2752;
        in.application_z = application_z;
        in.argument_3 = argument_3;
        in.argument_4 = argument_4;
        in.balance = frame.balance_2720;
        in.surface_speed = frame.surface_speed_2656;
        in.axis_384 = frame.axis_384;
        in.velocity_400 = frame.velocity_400;
        in.axis_544 = frame.axis_544;
        return in;
    };
    const auto first = ground_force::calculate(
        settings, common(frame.ground_scalar_1216, side_scalar, first_balance_selector(frame.balance_2720)));
    const auto second = ground_force::calculate(
        settings, common(-frame.ground_scalar_1240, frame.ground_scalar_1236 * side_scalar,
                         second_balance_selector(frame.balance_2720)));
    return {first, second};
}

template <class S>
rs::Result<rs::Unit, GroundBoardError<typename S::BoardError>> apply_body_vectors(
    PhysicsGroundState& state, float collision_scale, float timestep, std::array<float, 4> straighten,
    std::array<float, 4> heading, std::array<float, 4> anti_flip, std::array<float, 4> manual, S& services) {
    if (state.collision_countdown_2652 > 0.0f) {
        state.collision_countdown_2652 -= timestep;
        state.vector_2592 = rs::map(state.vector_2592, [collision_scale](float lane) { return lane * collision_scale; });
        S3_GB_TRY_UNIT(
            board::service(GroundBoardStage::ApplyCollisionDecay, services.apply_vector_82c07000(state.vector_2592)));
    }
    S3_GB_TRY_UNIT(board::service(GroundBoardStage::ApplyStraighten, services.apply_vector_82c07000(straighten)));
    S3_GB_TRY_UNIT(board::service(GroundBoardStage::ApplyHeading, services.apply_vector_82c07000(heading)));
    S3_GB_TRY_UNIT(
        board::service(GroundBoardStage::ApplyAntiFlip, services.apply_angular_displacement_82c075b8(anti_flip)));
    S3_GB_TRY_UNIT(
        board::service(GroundBoardStage::ApplyManual, services.apply_angular_displacement_82c075b8(manual)));
    return rs::Unit{};
}

template <class S>
rs::Result<bool, GroundBoardError<typename S::BoardError>> update_speed_model_reset(
    speed_model::SpeedModelState& speed_model, float threshold, Vector3 brake, Vector3 push,
    std::array<float, 8> pump, S& services) {
    const std::array<float, 4> combined{
        (push.x + brake.x) + pump[0],
        (push.y + brake.y) + pump[1],
        (push.z + brake.z) + pump[2],
        pump[3],
    };
    S3_GB_TRY(const float squared,
              board::service(GroundBoardStage::StrongForceSquared, services.dot3(combined, combined)));
    if (squared > threshold) {
        speed_model.flags_1360 |= 0x80000000u;
        return true;
    }
    return false;
}

}  // namespace detail

/// pub(super) in Rust.
template <class S>
rs::Result<OrdinaryGroundResult, GroundBoardError<typename S::BoardError>> update(
    PhysicsGroundState& state, GroundBoardComponents components, const GroundBoardSettings& settings,
    GroundBoardInput input, QueuedPointForce contact_force, S& services) {
    using Error = GroundBoardError<typename S::BoardError>;
    std::uint8_t suppressed = 0;
    const auto propulsion = propulsion::calculate(input.propulsion, settings.propulsion, suppressed);
    state.push_suppressed_2730 = suppressed != 0;

    const auto ground = detail::ground_forces(settings.ground_force, input.ground_force);
    const auto& ground_a = ground.first;
    const auto& ground_b = ground.second;
    input.speed_model.manual_state_276 = components.manual.angular_correction;
    const auto speed_force = speed_model::update(components.speed_model, settings.speed_model, input.speed_model);
    input.slide_friction.heading_time = state.elapsed_2648;
    // Both helpers read wheel hardness from ProcessedPhysIn+2764
    // (82D92AAC / 82D92B80), supplied by the animation profile.
    const auto slide_force = slide_friction::calculate(settings.slide_friction, input.slide_friction);
    const auto pump_force = pumping::calculate(input.pump_force);
    input.straighten.heading_time = state.elapsed_2648;
    const auto straighten_torque = straighten::calculate(settings.straighten, input.straighten);
    const float straighten_scale = state.straighten_scale_2672;
    const auto scaled_straighten =
        rs::map(straighten_torque, [straighten_scale](float lane) { return lane * straighten_scale; });
    const auto heading_torque = heading::calculate(settings.heading, input.heading, components.heading_previous);
    const auto anti_flip_torque = anti_flip::calculate(settings.anti_flip, input.anti_flip);
    state.anti_flip_torque_2624 = anti_flip_torque;

    input.manual.powersliding = false;
    auto manual_result = physics::manual::controller::calculate(components.manual, settings.manual,
                                                                settings.manual_mode, input.manual, services);
    if (manual_result.is_err()) {
        return rs::make_err(Error(typename Error::Manual{manual_result.unwrap_err()}));
    }
    const physics::manual::controller::ManualEffect manual = manual_result.unwrap();
    state.manual_correction_2732 = manual.correction_active;
    state.manual_opposition_2733 = manual.opposing_motion_without_correction;
    const float drag = input.ground_drag.calculate(settings.linear_drag);

    bool terminal_queued = false;
    if (manual.correction_active) {
        const PropulsionSubmission submission = propulsion.submit(manual, components.force_queue);
        if (const auto* queued = std::get_if<PropulsionSubmission::ManualCorrection>(&submission.value)) {
            terminal_queued = queued->_0;
        } else {
            S3_PANIC("internal error: entered unreachable code");
        }
    } else {
        const auto push = detail::vector4(propulsion.push.vector);
        S3_GB_TRY(const float push_squared,
                  board::service(GroundBoardStage::PushForceSquared, services.dot3(push, push)));
        if (push_squared > 1.0f) {
            state.flag_2721 = true;
        }
        S3_GB_TRY_UNIT(board::service(
            GroundBoardStage::AntiFlipNudge,
            corrections::update_anti_flip_nudge(state, input.anti_flip_nudge, components.force_queue, services)));
        if (input.contact_time_2756 < settings.ground_force_contact_time_limit) {
            components.force_queue.append(detail::force_record(4, ground_a));
            components.force_queue.append(detail::force_record(5, ground_b));
        }
        const PropulsionSubmission submission = propulsion.submit(manual, components.force_queue);
        if (!std::holds_alternative<PropulsionSubmission::BrakingAndPush>(submission.value)) {
            S3_PANIC("internal error: entered unreachable code");
        }
        S3_GB_TRY_UNIT(detail::apply_body_vectors(state, settings.collision_response_scale,
                                                  input.propulsion.timestep, scaled_straighten, heading_torque,
                                                  anti_flip_torque, manual.angular_displacement, services));
        if ((input.trajectory_state_1776_bits & 0x80000000u) == 0) {
            components.force_queue.append(detail::force_record(6, speed_force));
        }
        components.force_queue.append(detail::force_record(1, slide_force));
        components.force_queue.append(detail::force_record(8, pump_force));
        contact_force.tag = 16;
        terminal_queued = components.force_queue.append(contact_force);
    }

    {
        auto r = components.inertias.apply_ground_drag(drag);
        if (r.is_err()) return rs::make_err(Error(typename Error::Drag{r.unwrap_err()}));
    }
    S3_GB_TRY(const bool reset,
              detail::update_speed_model_reset(components.speed_model, settings.speed_model_reset_force_squared,
                                               propulsion.braking.force_world, propulsion.push.vector, pump_force,
                                               services));
    S3_GB_TRY_UNIT(
        board::service(GroundBoardStage::HangUps, corrections::manage_hang_ups(state, input.hang_up, services)));
    S3_GB_TRY_UNIT(board::service(GroundBoardStage::HalfpipeWheelCatch,
                                  corrections::manage_halfpipe_wheel_catches(input.halfpipe_wheel_catch, services)));
    S3_GB_TRY_UNIT(
        board::service(GroundBoardStage::Pinning, corrections::consider_pinning(state, input.pinning, services)));
    OrdinaryGroundResult result;
    result.manual_correction = manual.correction_active;
    result.terminal_force_tag = manual.correction_active ? 7u : 16u;
    result.terminal_force_queued = terminal_queued;
    result.speed_model_reset = reset;
    return result;
}

}  // namespace s3::riding::grounded::state::ordinary
