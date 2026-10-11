// Port of skate-core/src/animation/foot_ik/status.rs.
// Limb mode transitions82BEDCF8 and blend weights82BEEC00.
// Named S2 UpdateStatus/UpdateBlendValues corroborate field identities;
// TU3 flags and cached settings determine the implementation.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/rs.hpp"

namespace s3::animation::foot_ik::status {

enum class Mode {
    Disabled,
    OnDeck,
    External,
    Local,
};

/// Manual Rust `Default`: member initializers == `LimbStatus::default_()`.
struct LimbStatus {
    //82BED780 resets mode, both weights, target blend, target flags and
    //the complete local delta/part-position vectors to zero.
    Mode mode = Mode::Disabled;
    /// SkeletonIK+468+4*limb.
    float board_blend = 0.0f;
    /// SkeletonIK+484+4*limb.
    float external_blend = 0.0f;
    float target_blend = 0.0f;
    bool external_target_set = false;
    bool local_target_set = false;
    std::array<float, 4> external_target_local_delta{};
    /// CalculateInitialPartTransforms retains the target in animation-deck space.
    std::array<float, 4> part_position{};

    static LimbStatus default_() { return LimbStatus{}; }
};

/// All four limbs in native order: left foot, right foot, left hand, right hand.
/// Foot enable is SkeletonIK464; reset82BED780 initializes it to true.
void update_modes(std::array<LimbStatus, 4>& limbs, bool feet_enabled, std::uint32_t flags_2472);

struct BlendSettings {
    /// physics_skeletonik cached layout16/32.
    std::array<float, 4> hand_inner_padding{};
    std::array<float, 4> hand_outer_padding{};
    /// Cached layout84 and88; native increments per update, without dt.
    float external_blend_step = 0.0f;
    float board_blend_step = 0.0f;
};

/// Board dimensions are the actual constructor-derived half width and total
/// half length, not the board's visual bounds or cached generic dimensions.
void update_blends(std::array<LimbStatus, 4>& limbs, float deck_half_width, float deck_total_half_length,
                   std::uint32_t flags_2480, const BlendSettings& settings);

}  // namespace s3::animation::foot_ik::status
