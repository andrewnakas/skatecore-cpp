// Port of skate-core/src/player/input_phase/pose_output.rs (whole file).
// Completed pose observations; original PhysOut template constructors.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/player/input_phase/raw.hpp"
#include "s3/core/rs.hpp"

namespace s3::physics::skeleton_body::record {
struct SkeletonPhysicalRecord;
}  // namespace s3::physics::skeleton_body::record

namespace s3::player::input_phase::pose_output {

using types::RawMatrix;

/// Skeleton reset82DE3A28 explicitly clears these scalars and flags.
struct SkeletonOutputFields {
    float hips_right_angle_496 = 0.0f;
    float hips_up_angle_500 = 0.0f;
    /// Skeleton Fill82BE2268: signed physical twist, before animation mirroring.
    float twist_504 = 0.0f;
    /// Skeleton Fill82BE1C84/82BE1D50: animation deck forward, stance-adjusted.
    float deck_yaw_536 = 0.0f;
    float deck_pitch_540 = 0.0f;
    float scalar_544 = 0.0f;
    float no_support_time_548 = 0.0f;
    float time_until_teleport_576 = 0.0f;
    float response_strength_580 = 0.0f;
    float extra_weight_584 = 0.0f;
    float response_change_588 = 0.0f;
    std::uint8_t flag_597 = 0;
    std::uint8_t over_599 = 0;
    std::uint8_t flag_600 = 0;
    std::uint8_t flag_601 = 0;
    std::uint8_t teleport_pending_604 = 0;
    std::uint8_t response_changed_605 = 0;
    RawMatrix anim_to_world_11920{};

    /// Fill82BE1AE8: Skeleton6512 is animation-record pose0's forward column.
    /// Packet10368 flips that vector; pitch is its dot with world up, not asin.
    void publish_deck_angles(std::array<float, 4> forward, bool board_flipped);

    /// Skeleton Fill82BE2148..2268. Read the completed physical record, the
    /// current input toolkit's Processed352 and Reckoning1152. No pose fitting
    /// or animation mirror: GrindControlFade applies that mirror once on Begin.
    void publish_twist(const physics::skeleton_body::record::SkeletonPhysicalRecord& record,
                       std::array<float, 4> processed_forward_352, std::array<float, 4> reckoning_up_1152);
};
S3_DERIVE_EQ(SkeletonOutputFields, hips_right_angle_496, hips_up_angle_500, twist_504, deck_yaw_536, deck_pitch_540,
             scalar_544, no_support_time_548, time_until_teleport_576, response_strength_580, extra_weight_584,
             response_change_588, flag_597, over_599, flag_600, flag_601, teleport_pending_604,
             response_changed_605, anim_to_world_11920)

struct AnimationOutputFields {
    /// Animation82DE3F38 stores FLT_MAX at144 and zero at148.
    float collision_time_144 = rs::f32::MAX;
    std::uint32_t profile_148 = 0;
    std::uint8_t tricks_blocked_on_stairs_166 = 0;
    /// Ground Fill82D3A4F0 -> PhysicsWantsManualExit82BA7988.
    std::uint8_t manual_opposition_168 = 0;
};
S3_DERIVE_EQ(AnimationOutputFields, collision_time_144, profile_148, tricks_blocked_on_stairs_166,
             manual_opposition_168)

/// Scoring2 reset82DE4468 clears the conditioner capability mask at204.
struct ScoringOutputFields {
    std::uint32_t capabilities_204 = 0;
};
S3_DERIVE_EQ(ScoringOutputFields, capabilities_204)

}  // namespace s3::player::input_phase::pose_output
