// Port of skate-core/src/air/reckoning/data.rs (whole file).
// Additional persistent Reckoning fields and actual inputs for82D8DBD8.
#pragma once
#include <array>

#include "s3/core/air/body_flip.hpp"
#include "s3/core/air/body_spin.hpp"
#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/core/point_graph.hpp"

namespace s3::air::reckoning::data {

using physics::skeleton_animation_record::AnimationPartTransform;
using physics::skeleton_animation_record::IDENTITY;
using point_graph::PointGraph;

struct AirState {
    /// 1568/1572, shared with Ground entry and PhysicsAir exit.
    float spin_angle = 0.0f;
    float spin_speed = 0.0f;
    /// 1580 is cleared by UpdateAirStates (1576 lives in ReckoningFrames).
    float secondary_lean_angle = 0.0f;
    float flip_angle = 0.0f;
    float flip_speed = 0.0f;
    float flip_requested_speed = 0.0f;
    /// 1072; final combined1008 has one owner, ReckoningFrames.body_flip.
    AnimationPartTransform spin_transform = IDENTITY;
    /// 1232 and1596; their native trajectory/entry producers remain the caller.
    std::array<float, 4> flip_axis{1.0f, 0.0f, 0.0f, 0.0f};
    bool flip_active = false;
    /// 1597, BeginBodyFlip82D8EC60 and ResetBodyFlip82D8EAA4.
    bool flip_side = false;

    /// 82D33178: scalar zeros, identity1072, world-X axis and inactive flip.
    static AirState new_() { return AirState{}; }
    /// Ground entry82D37538/PhysicsAir exit82D346A0 reset exactly this pair.
    void reset_spin() {
        spin_angle = 0.0f;
        spin_speed = 0.0f;
    }
};
S3_DERIVE_EQ(AirState, spin_angle, spin_speed, secondary_lean_angle, flip_angle, flip_speed,
             flip_requested_speed, spin_transform, flip_axis, flip_active, flip_side)

struct Settings {
    std::array<float, 4> ground_normal_smoothing{};
    /// PointNegGraphData8 layout336; caller directly evaluates its X/Y352/384.
    PointGraph<8> max_up_angle_delta{};
    PointGraph<8> tilt_vs_rotation{};
    PointGraph<8> tilt_vs_slope{};
    body_spin::BodySpinSettings body_spin;
    body_flip::BodyFlipSettings body_flip;
};

struct Input {
    std::array<float, 4> landing_normal{};
    float normal_blend = 0.0f;
    float target_spin = 0.0f;
    float flip_request = 0.0f;
    /// Processed752, from the actual Skeleton animation COM-to-deck producer.
    std::array<float, 4> com_to_deck{};
    float timestep = 0.0f;
    /// Processed2812, live PhysBodySpin animation attribute.
    float physical_body_spin = 0.0f;
    /// Processed2644, produced by Skeleton82BDD008; used only when2484bit15 is set.
    float grind_adjusted_body_spin = 0.0f;
    bool additive_spin = false;
    bool direct_spin = false;
    bool reverse_stance = false;
    /// Actual selected physics_mode layout108 and28.
    bool easy_body_spins = false;
    bool perfect_body_flips = false;
};

}  // namespace s3::air::reckoning::data
