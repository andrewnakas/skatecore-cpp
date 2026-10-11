// Port of skate-core/src/riding/push_behaviors.rs.
// Push behavior state and coefficient calculations from Skate 3 TU3.
// Animation-tree selection and graph transitions belong to the authored graph.
#pragma once
#include <array>
#include <optional>

#include "s3/core/riding/push_animation.hpp"

namespace s3::riding::push_behaviors {

using push_animation::PushAnimationCurves;
using push_animation::PushBlendParameters;

struct PushClipMetrics {
    float length = 0.0f;
    float begin_velocity = 0.0f;
    float end_velocity = 0.0f;
};
S3_DERIVE_EQ(PushClipMetrics, length, begin_velocity, end_velocity)

struct PushAttributes {
    /// Native order: low-speed/high-strength, high-speed/high-strength,
    /// low-speed/low-strength, high-speed/low-strength.
    std::array<PushClipMetrics, 4> clips{};
    float minimum_begin_velocity = 0.0f;
    float maximum_begin_velocity = 0.0f;
    float minimum_delta_velocity = 0.0f;
    float maximum_delta_velocity = 0.0f;

    /// 82B954D8, after 82B95620 obtains each tree's length and its last
    /// HStr_Vel_B/LStr_Vel_B and Vel_E attributes. No fitted clip coefficients.
    static PushAttributes from_clips(std::array<PushClipMetrics, 4> clips);

    /// ComputeTargetCoefsFromSpeedAndStrength, 82BAD260. The blend accounts
    /// for differing clip lengths; a linear velocity interpolation is wrong.
    PushBlendParameters target(float forward_speed, float strength) const;

    /// ComputeRepushDeadline::End, 82BADA90. Despite its graph name, this
    /// writes PushState.out_factor, not a time deadline or a graph transition.
    float out_factor(float forward_speed, float strength, float speed_weight, float maximum_out) const;
};
S3_DERIVE_EQ(PushAttributes, clips, minimum_begin_velocity, maximum_begin_velocity, minimum_delta_velocity,
             maximum_delta_velocity)

struct PushState {
    float out_factor = 0.0f;
    float current_push_dv = 0.0f;
    PushBlendParameters current{};
    PushBlendParameters target{};
    bool continue_push = false;

    /// InitPush::Begin, 82BAD5B8, preserves out_factor.
    void initialize_push();
};
S3_DERIVE_EQ(PushState, out_factor, current_push_dv, current, target, continue_push)

struct FirstPushStrength {
    float simulated_held_seconds = 0.0f;
    bool push_from_teleport = false;
    bool first_push = false;
    bool first_update = false;

    /// Begin82BAD6E8: capture teleport eligibility once per activation.
    static FirstPushStrength begin(float time_since_teleport, float teleport_window);
    /// Update82BAD7A8: the teleport path evaluates the old simulated time
    /// before adding dt. A released first push cannot be resumed here.
    void update(PushState& state, const PushAnimationCurves& curves, std::optional<float> pushing,
                float forward_speed, float dt);
};
S3_DERIVE_EQ(FirstPushStrength, simulated_held_seconds, push_from_teleport, first_push, first_update)

/// A snapshot of the authored MG intent list, not raw controller buttons.
struct PushIntents {
    std::optional<float> pushing;
    bool new_push = false;
    /// Presence of the configured newPushName (RightPush or LeftPush in stock).
    bool configured_foot = false;
};
S3_DERIVE_EQ(PushIntents, pushing, new_push, configured_foot)

enum class PushCyclePhase {
    HoldingFirst,
    WaitingNext,
    HoldingNext,
    NextReleased,
};

struct PushCycle {
    PushCyclePhase phase = PushCyclePhase::HoldingFirst;
    float next_push_dv = 0.0f;

    /// Begin82BADCD0.
    static PushCycle begin(PushState& state, const PushAnimationCurves& curves, PushIntents intents);
    /// Update82BADE38. Separate phase checks intentionally allow the release
    /// and next-press path to run in one graph update, as in the recovered code.
    void update(PushState& state, const PushAnimationCurves& curves, PushIntents intents, float forward_speed,
                float maximum_holding_acceleration);
};
S3_DERIVE_EQ(PushCycle, phase, next_push_dv)

/// Physics outputs consumed by GetFootPos82595B28. All positions are world
/// space; deck axes come from the live reckoning transform, not a render pose.
struct PushFootFrame {
    std::array<float, 4> left_foot{};
    std::array<float, 4> right_foot{};
    std::array<float, 4> deck_position{};
    std::array<float, 4> deck_y{};
    std::array<float, 4> deck_z{};
    bool skateboard_flipped = false;

    /// TU382595B28. Reuses the isolated, approximation-derived Xenon dot
    /// lowering; independent hardware arithmetic validation remains pending.
    std::array<float, 2> out_distance(bool right_foot) const;
};
S3_DERIVE_EQ(PushFootFrame, left_foot, right_foot, deck_position, deck_y, deck_z, skateboard_flipped)

}  // namespace s3::riding::push_behaviors
