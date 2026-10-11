// Port of skate-core/src/riding/grounded/state/corrections.rs.
// Ground correction paths called by `82D38800`.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/math.hpp"
#include "s3/core/physics/force_queue.hpp"
#include "s3/core/riding/grounded/state/data.hpp"

namespace s3::riding::grounded::state::corrections {

using data::PhysicsGroundState;
using math::Vector3;
using physics::force_queue::BoardForceQueue;
using physics::force_queue::QueuedPointForce;

namespace detail {
inline float flip_sign_bit(float value) { return rs::from_bits(rs::to_bits(value) ^ 0x80000000u); }
inline Vector3 xyz(const std::array<float, 4>& value) { return Vector3{value[0], value[1], value[2]}; }
}  // namespace detail

// `?` for rs::Result: early-return the error, otherwise bind the value.
#define S3_CORR_TRY(decl, expr)                                       \
    auto S3_PP_CAT(s3_corr_r_, __LINE__) = (expr);                    \
    if (S3_PP_CAT(s3_corr_r_, __LINE__).is_err())                     \
        return rs::make_err(S3_PP_CAT(s3_corr_r_, __LINE__).unwrap_err()); \
    decl = S3_PP_CAT(s3_corr_r_, __LINE__).unwrap()
#define S3_CORR_TRY_UNIT(expr)                                        \
    do {                                                              \
        auto s3_corr_u_ = (expr);                                     \
        if (s3_corr_u_.is_err()) return rs::make_err(s3_corr_u_.unwrap_err()); \
    } while (0)

struct AntiFlipNudgeInput {
    float deck_speed_2652 = 0.0f;
    std::array<float, 4> deck_axis_96{};
};
S3_DERIVE_EQ(AntiFlipNudgeInput, deck_speed_2652, deck_axis_96)

/// Rust trait `AntiFlipNudgeMath` (duck-typed template parameter `M`):
///   using Error = ...;
///   /// Exact `vmsum3fp128` operation. Generic host dot arithmetic is not a
///   /// substitute for the recovered Xenon operation.
///   rs::Result<float, Error> dot3(std::array<float,4> left, std::array<float,4> right);
///   /// Exact `vrsqrtefp128` plus two refinements and final scale at
///   /// `82D39498..82D394EC`.
///   rs::Result<std::array<float,4>, Error> scale_to_magnitude(std::array<float,4> vector,
///                                                             float squared_length, float magnitude);

struct AntiFlipNudgeResult {
    bool attempted = false;
    bool queued = false;
};
S3_DERIVE_EQ(AntiFlipNudgeResult, attempted, queued)

/// Complete state/branch/queue contract of `UpdateAntiFlipNudge` `82D39338`.
template <class M>
rs::Result<AntiFlipNudgeResult, typename M::Error> update_anti_flip_nudge(PhysicsGroundState& state,
                                                                          AntiFlipNudgeInput input,
                                                                          BoardForceQueue& queue, M& math) {
    S3_CORR_TRY(const float anti_flip_squared, math.dot3(state.anti_flip_torque_2624, state.anti_flip_torque_2624));
    state.anti_flip_nudge_frames_2752 =
        anti_flip_squared > 0.0f ? std::int32_t(std::uint32_t(state.anti_flip_nudge_frames_2752) + 1u)
                                 // `ble` after `fcmpu` also takes the unordered case.
                                 : 0;
    if (state.anti_flip_nudge_frames_2752 <= 12 || !(input.deck_speed_2652 < rs::from_bits(0x3f8ccccdu))) {
        return AntiFlipNudgeResult{false, false};
    }

    std::array<float, 4> direction = input.deck_axis_96;
    if (direction[1] > 0.0f) {
        direction = rs::map(direction, detail::flip_sign_bit);
    }
    direction[1] = 0.0f;
    S3_CORR_TRY(const float squared, math.dot3(direction, direction));
    if (!(squared > rs::from_bits(0x3a83126fu))) {
        return AntiFlipNudgeResult{false, false};
    }
    S3_CORR_TRY(const auto force, math.scale_to_magnitude(direction, squared, 50.0f));
    const bool queued = queue.append(QueuedPointForce{17, detail::xyz(force), Vector3::ZERO});
    // The native byte is set after the append call regardless of capacity.
    state.anti_flip_nudge_applied_2723 = true;
    return AntiFlipNudgeResult{true, queued};
}

struct HangUpInput {
    std::uint32_t flags_1516 = 0;
    float deck_speed_2652 = 0.0f;
    float scalar_84 = 0.0f;
    /// Exact `vmsum3fp128` comparison at `82D39744..82D39764`.
    bool geometry_axis_dot_positive = false;
};
S3_DERIVE_EQ(HangUpInput, flags_1516, deck_speed_2652, scalar_84, geometry_axis_dot_positive)

/// Rust trait `HangUpServices` (duck-typed template parameter `S`):
///   using Error = ...;
///   /// `82D37048`, `82D370F8` and the direction/up-vector construction at
///   /// `82D395AC..82D396AC`, including its signed side test.
///   rs::Result<std::array<float,4>, Error> build_hang_force();
///   /// Direct `ApplyWorldSpaceForceToDeck` call at `82D396DC`.
///   rs::Result<rs::Unit, Error> apply_hang_force(std::array<float,4> force);
///   /// Exact `82C20530/5D8/728/C08/6C0` geometry-object lifetime and result.
///   rs::Result<bool, Error> detect_hung_up_geometry();
///   rs::Result<rs::Unit, Error> request_wipeout();

/// Complete counter and effect order of `ManageHangUps` `82D39510`.
template <class S>
rs::Result<rs::Unit, typename S::Error> manage_hang_ups(PhysicsGroundState& state, HangUpInput input, S& services) {
    const bool investigating = (input.flags_1516 & 0x08000000u) != 0;
    if (investigating) {
        if (input.deck_speed_2652 < rs::from_bits(0x3f8ccccdu) && input.scalar_84 < rs::from_bits(0x3f666666u) &&
            input.scalar_84 > 0.0f) {
            state.hang_detection_frames_2740 = std::int32_t(std::uint32_t(state.hang_detection_frames_2740) + 1u);
        }
    } else {
        state.hang_detection_frames_2740 = 0;
    }

    if (state.hang_detection_frames_2740 == 20) {
        state.hang_force_frames_2744 = 10;
        S3_CORR_TRY(state.vector_2608, services.build_hang_force());
    }
    if (state.hang_force_frames_2744 > 0) {
        if (investigating) {
            S3_CORR_TRY_UNIT(services.apply_hang_force(state.vector_2608));
        }
        state.hang_force_frames_2744 = std::int32_t(std::uint32_t(state.hang_force_frames_2744) - 1u);
        if (state.hang_force_frames_2744 == 0) {
            state.hang_detection_frames_2740 = 0;
        }
    }

    const bool geometry_candidate = input.deck_speed_2652 < rs::from_bits(0x3f8ccccdu) &&
                                    (input.flags_1516 & 0x0c000000u) == 0x0c000000u &&
                                    input.geometry_axis_dot_positive;
    bool hung = false;
    if (geometry_candidate) {
        S3_CORR_TRY(hung, services.detect_hung_up_geometry());
    }
    state.hung_wipeout_frames_2748 =
        hung ? std::int32_t(std::uint32_t(state.hung_wipeout_frames_2748) + 1u) : 0;
    if (state.hung_wipeout_frames_2748 > 20) {
        S3_CORR_TRY_UNIT(services.request_wipeout());
    }
    return rs::Unit{};
}

struct HalfpipeWheelCatchInput {
    std::uint32_t flags_1516 = 0;
    float deck_speed_2652 = 0.0f;
    float deck_x_axis_y = 0.0f;
    float deck_y_axis_y = 0.0f;
    float signed_deck_distance = 0.0f;
};
S3_DERIVE_EQ(HalfpipeWheelCatchInput, flags_1516, deck_speed_2652, deck_x_axis_y, deck_y_axis_y,
             signed_deck_distance)

/// Rust trait `HalfpipeWheelCatchServices` (duck-typed template parameter `S`):
///   using Error = ...;
///   /// Exact signed axis/cross-product construction at `82D39950..82D399E8`.
///   rs::Result<std::array<float,4>, Error> angular_displacement();
///   rs::Result<rs::Unit, Error> apply_angular_displacement(std::array<float,4> value);

/// Gating and effect order of `ManageHalfpipeWheelCatches` `82D39868`.
template <class S>
rs::Result<bool, typename S::Error> manage_halfpipe_wheel_catches(HalfpipeWheelCatchInput input, S& services) {
    const bool candidate = (input.flags_1516 & 0x08000000u) != 0 && input.deck_speed_2652 < 3.0f &&
                           rs::abs(input.deck_x_axis_y) < rs::from_bits(0x3f11eb85u) &&
                           rs::abs(input.deck_y_axis_y) < rs::from_bits(0x3df5c28fu) &&
                           input.signed_deck_distance < rs::from_bits(0x3de147aeu) &&
                           input.signed_deck_distance > 0.0f;
    if (!candidate) {
        return false;
    }
    S3_CORR_TRY(const auto displacement, services.angular_displacement());
    S3_CORR_TRY_UNIT(services.apply_angular_displacement(displacement));
    return true;
}

struct PinningInput {
    std::uint32_t flags_2488 = 0;
    std::int32_t frames_since_teleport_2584 = 0;
    std::uint32_t flags_2472 = 0;
};
S3_DERIVE_EQ(PinningInput, flags_2488, frames_since_teleport_2584, flags_2472)

/// Rust trait `PinningServices` (duck-typed template parameter `S`):
///   using Error = ...;
///   /// Exact reciprocal-refinement velocity and `82C04168` application at
///   /// `82D39B04..82D39B8C` using captured X/Z, live Y, position and timestep.
///   rs::Result<rs::Unit, Error> pin_to_captured_position(float captured_x, float captured_z);

/// Complete state gating of `ConsiderPinning` `82D39A18`.
template <class S>
rs::Result<rs::Unit, typename S::Error> consider_pinning(PhysicsGroundState& state, PinningInput input, S& services) {
    if ((input.flags_2488 & 1u) != 0) {
        state.pinning_2727 = false;
        state.was_pinning_2728 = false;
        return rs::Unit{};
    }
    if (state.human_player_2724 && input.frames_since_teleport_2584 < 90) {
        const bool eligible = state.was_pinning_2728 || input.frames_since_teleport_2584 < 30;
        const bool controls_seen = input.frames_since_teleport_2584 > 0 && (input.flags_2472 & 0x00800000u) == 0;
        state.controls_latched_2725 = state.controls_latched_2725 || controls_seen;
        if (eligible && state.captured_position_valid_2726 && !state.controls_latched_2725) {
            S3_CORR_TRY_UNIT(
                services.pin_to_captured_position(state.captured_position_x_2656, state.captured_position_z_2660));
            state.pinning_2727 = true;
        }
    }
    state.was_pinning_2728 = state.pinning_2727;
    return rs::Unit{};
}

#undef S3_CORR_TRY
#undef S3_CORR_TRY_UNIT

}  // namespace s3::riding::grounded::state::corrections
