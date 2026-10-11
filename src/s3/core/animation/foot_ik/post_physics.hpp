// Port of skate-core/src/animation/foot_ik/post_physics.rs.
// PostWipeoutCheck's feet82BF0680, hands82BF0C28 and external82BEDC10.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/animation/foot_ik/drive.hpp"
#include "s3/core/animation/foot_ik/post_contact.hpp"
#include "s3/core/animation/foot_ik/settings.hpp"
#include "s3/core/animation/foot_ik/state.hpp"
#include "s3/core/physics/skeleton_body.hpp"

namespace s3::animation::foot_ik::post_physics {

using Transform = physics::skeleton_animation_record::AnimationPartTransform;

struct Input {
    std::uint32_t state_id = 0;
    std::uint32_t category_id = 0;
    ///Actual BoardBody868, used by original state200/201 hand/foot gates.
    bool board_body_flag_868 = false;
    bool wipeout = false;
    std::uint32_t flags_2468 = 0;
    std::uint32_t flags_2484 = 0;
    ///Processed2664 and2520, read only when2468bit18 is set.
    float value_2664 = 0.0f;
    std::uint32_t state_2520 = 0;
    const Transform& world_to_animation;
    ///Actual board GetPartTransform, replaced by wobble-adjusted record when sampled.
    const Transform& board;
};

std::array<bool, 4> update(state::FootIkState& state, physics::skeleton_body::SkeletonBody& body,
                           const drive::Geometry& geometry, const settings::Settings& settings,
                           const post_contact::SettingsPost& post, const Input& input);

}  // namespace s3::animation::foot_ik::post_physics
