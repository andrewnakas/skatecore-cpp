// Port of skate-core/src/riding/grounded/state/data.rs.
// Persistent fields owned by TU3 `PhysState_PhysicsGround`.
//
// Offsets remain in names when the original semantic name has not been
// recovered. This prevents an adapter from silently assigning an invented
// meaning to a stock field.
#pragma once
#include <array>
#include <cstdint>
#include <optional>

#include "s3/core/riding/grounded/state/contact_state.hpp"

namespace s3::riding::grounded::state::data {

using contact_state::GroundContactStateInput;

struct GroundUpdateInput {
    std::int32_t wheel_contact_count_2556 = 0;
    std::array<float, 4> position_112{};
    float timestep_2604 = 0.0f;
    GroundContactStateInput contact_state;
};
S3_DERIVE_EQ(GroundUpdateInput, wheel_contact_count_2556, position_112, timestep_2604, contact_state)

struct GroundUpdateSignals {
    bool set_skeleton_flag_16505 = false;
};
S3_DERIVE_EQ(GroundUpdateSignals, set_skeleton_flag_16505)

struct PhysicsGroundState {
    /// Collision-force record retained by the successful `82D944E8` branch.
    std::array<float, 4> collision_force_2528{};
    std::array<float, 4> collision_point_2544{};
    std::uint32_t word_2560 = 0;
    std::uint32_t word_2564 = 0;
    /// Collision response retained and decayed by `82D38800`.
    std::array<float, 4> vector_2592{};
    /// Direct hang-up force assembled by `82D39510`.
    std::array<float, 4> vector_2608{};
    /// Output of `Toolkit_AntiFlipTorque` (`82D94190`).
    std::array<float, 4> anti_flip_torque_2624{};
    /// Optional steering history arguments passed to `82D92440`.
    float steering_push_scalar_2640 = 0.0f;
    float steering_damped_turn_2644 = 0.0f;
    /// Accumulated by ProcessedPhysIn+2604 once per Ground Update.
    float elapsed_2648 = 0.0f;
    /// Collision-response countdown set from the selected global settings.
    float collision_countdown_2652 = 0.0f;
    /// First contacted position captured once for spawn pinning.
    float captured_position_x_2656 = 0.0f;
    float captured_position_z_2660 = 0.0f;
    /// Time remaining before the skater may enter the skitch path.
    float scalar_2664 = 0.0f;
    /// Height of the selected skitch spline.
    float scalar_2668 = 0.0f;
    /// Multiplies the straighten-out torque before heading calculation.
    float straighten_scale_2672 = 0.0f;
    /// Animated-board velocity/displacement state used by Enter/Exit and Update.
    std::array<float, 4> vector_2688{};
    /// Explicitly zeroed by `82D93DF0` through its Ground+2688 output record.
    float scalar_2704 = 0.0f;
    /// Selects the animated-board branch of `82D38800`.
    bool flag_2708 = false;
    bool flag_2720 = false;
    bool flag_2721 = false;
    bool flag_2722 = false;
    bool anti_flip_nudge_applied_2723 = false;
    /// Pinning is restricted to the human-player path.
    bool human_player_2724 = false;
    bool controls_latched_2725 = false;
    bool captured_position_valid_2726 = false;
    bool pinning_2727 = false;
    bool was_pinning_2728 = false;
    bool flag_2729 = false;
    bool push_suppressed_2730 = false;
    bool flag_2731 = false;
    bool manual_correction_2732 = false;
    bool manual_opposition_2733 = false;
    std::int32_t hang_detection_frames_2740 = 0;
    std::int32_t hang_force_frames_2744 = 0;
    std::int32_t hung_wipeout_frames_2748 = 0;
    std::int32_t anti_flip_nudge_frames_2752 = 0;

    /// Ground constructor82D37358..37534. Fields whose first native write is
    /// Enter are safe host zero storage until Enter replaces them; this method
    /// does not imply that entering Ground has already occurred.
    static PhysicsGroundState before_first_enter(bool human_player) {
        PhysicsGroundState s{};
        s.human_player_2724 = human_player;
        return s;
    }

    /// State-owned prefix82D3758C..5C0 after the physical collision mode change.
    void begin_entry() {
        word_2560 = 0;
        word_2564 = 0;
        flag_2708 = false;
        controls_latched_2725 = false;
        captured_position_valid_2726 = false;
        was_pinning_2728 = false;
        flag_2731 = false;
        manual_correction_2732 = false;
        manual_opposition_2733 = false;
    }
    /// State-owned tail82D378AC..7914, after pumping and controller-mode reset.
    void finish_entry(std::uint32_t previous_state) {
        straighten_scale_2672 = previous_state == 101 ? rs::from_bits(0x3e23d70au) : 1.0f;
        steering_push_scalar_2640 = 1.0f;
        steering_damped_turn_2644 = 0.0f;
        elapsed_2648 = 0.0f;
        collision_countdown_2652 = 0.0f;
        vector_2592 = {0.0f, 0.0f, 0.0f, 0.0f};
        vector_2608 = {0.0f, 0.0f, 0.0f, 0.0f};
        anti_flip_torque_2624 = {0.0f, 0.0f, 0.0f, 0.0f};
        hang_detection_frames_2740 = 0;
        hang_force_frames_2744 = 0;
        hung_wipeout_frames_2748 = 0;
        anti_flip_nudge_frames_2752 = 0;
        flag_2721 = false;
        anti_flip_nudge_applied_2723 = false;
        push_suppressed_2730 = false;
    }

    /// Prefix of `Update` `82D37CB4..82D37D28`, before contact-state work.
    GroundUpdateSignals begin_update(GroundUpdateInput input) {
        return begin_update_at(input.wheel_contact_count_2556, input.position_112);
    }

    GroundUpdateSignals begin_update_at(std::int32_t wheel_contacts, std::array<float, 4> position) {
        if (!captured_position_valid_2726 && wheel_contacts > 0) {
            captured_position_x_2656 = position[0];
            captured_position_z_2660 = position[2];
            captured_position_valid_2726 = true;
        }
        anti_flip_torque_2624 = {0.0f, 0.0f, 0.0f, 0.0f};
        flag_2722 = false;
        anti_flip_nudge_applied_2723 = false;
        pinning_2727 = false;
        scalar_2664 = -1.0f;
        flag_2729 = false;
        return GroundUpdateSignals{elapsed_2648 > rs::from_bits(0x3d4ccccdu)};
    }

    /// Tail of `Update` `82D37EBC..82D37EEC` after prediction succeeds.
    void finish_update(float timestep_2604) { elapsed_2648 += timestep_2604; }
};
// derive(PartialEq): 36 fields, split over two S3_EQF chains.
[[maybe_unused]] inline bool operator==(const PhysicsGroundState& a, const PhysicsGroundState& b) {
    return S3_EQF_20(collision_force_2528, collision_point_2544, word_2560, word_2564, vector_2592, vector_2608,
                     anti_flip_torque_2624, steering_push_scalar_2640, steering_damped_turn_2644, elapsed_2648,
                     collision_countdown_2652, captured_position_x_2656, captured_position_z_2660, scalar_2664,
                     scalar_2668, straighten_scale_2672, vector_2688, scalar_2704, flag_2708, flag_2720) &&
           S3_EQF_16(flag_2721, flag_2722, anti_flip_nudge_applied_2723, human_player_2724, controls_latched_2725,
                     captured_position_valid_2726, pinning_2727, was_pinning_2728, flag_2729, push_suppressed_2730,
                     flag_2731, manual_correction_2732, manual_opposition_2733, hang_detection_frames_2740,
                     hang_force_frames_2744, hung_wipeout_frames_2748) &&
           (a.anti_flip_nudge_frames_2752 == b.anti_flip_nudge_frames_2752);
}
[[maybe_unused]] inline bool operator!=(const PhysicsGroundState& a, const PhysicsGroundState& b) {
    return !(a == b);
}

/// Contact identity history updated by `82D38430`. The concrete collision
/// object handle is supplied by the world adapter rather than stored as a host
/// pointer in the engine-independent core.
template <class T>
struct GroundContactHistory {
    std::int32_t differing_contact_frames_2756 = 0;
    std::optional<T> tracked_contact_2760;

    static constexpr GroundContactHistory empty() { return GroundContactHistory{0, std::nullopt}; }

    bool operator==(const GroundContactHistory& o) const {
        return differing_contact_frames_2756 == o.differing_contact_frames_2756 &&
               tracked_contact_2760 == o.tracked_contact_2760;
    }
    bool operator!=(const GroundContactHistory& o) const { return !(*this == o); }
};

}  // namespace s3::riding::grounded::state::data
