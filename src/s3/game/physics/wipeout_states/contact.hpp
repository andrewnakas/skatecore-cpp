// Port of skate-game/src/physics/wipeout_states/contact.rs
//! Wipeout300's material corrections and collision-policy requests82D3EC08.
#pragma once
#include <array>

#include "s3/core/physics/skeleton_body.hpp"
#include "s3/core/player/wipeout_state.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/skeleton_controller.hpp"
#include "s3/game/physics/wipeout_states/ragdoll.hpp"

namespace s3::game::physics::wipeout_states::contact {

Status update(::s3::player::wipeout_state::State& state,
              ::s3::player::wipeout_state::contact_response::ContactResponse& response,
              ::s3::physics::skeleton_body::SkeletonBody& body,
              const ::s3::physics::skeleton_body::SkeletonCollisionFeedback& feedback,
              skeleton_controller::SkeletonControllerState& controller,
              ::s3::physics::skeleton_body::SkeletonCollisionMode& collision,
              ::s3::physics::skeleton_body::SkeletonJoints& joints, const ragdoll::RagdollSetup& ragdoll,
              bool contact, std::array<float, 4> com_velocity, std::array<float, 4> effective_axis,
              std::array<float, 2> controls);

}  // namespace s3::game::physics::wipeout_states::contact
