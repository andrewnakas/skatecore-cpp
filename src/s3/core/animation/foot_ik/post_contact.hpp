// Port of skate-core/src/animation/foot_ik/post_contact.rs.
// SkeletonIK::CheckFootAgainstBoard82BF0D70, including raised deck ends.
#pragma once
#include <array>
#include <optional>

#include "s3/core/animation/foot_ik/settings.hpp"
#include "s3/core/physics/skeleton_animation_record.hpp"

namespace s3::animation::foot_ik::post_contact {

using Transform = physics::skeleton_animation_record::AnimationPartTransform;
using settings::Settings;

struct SettingsPost {
    ///physics_animation/default layout780.
    float minimum_board_up = 0.0f;
    ///Global212's authored layout412/416, selected by the wipeout argument.
    float wipeout_height = 0.0f;
    float riding_height = 0.0f;
};

/// pub(super) in Rust.
std::optional<std::array<float, 4>> target(const Transform& board, std::array<float, 4> foot,
                                           std::array<float, 4> adjacent,
                                           std::array<float, 4> animated_board_position, bool wipeout,
                                           const Settings& settings, const SettingsPost& post);

///Scalar constructor path through original8252D980. Its argument is already
///clamped to [.01,1.5607964]; coefficients come from822FE3C0..E0. (Private in Rust.)
float tangent(float angle);

}  // namespace s3::animation::foot_ik::post_contact
