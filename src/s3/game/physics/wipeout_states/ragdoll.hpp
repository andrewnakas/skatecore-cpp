// Port of skate-game/src/physics/wipeout_states/ragdoll.rs
//! Physical-owner adapter for selector82D913C0 requests8..11 and82BE7280.
#pragma once
#include <cstdint>
#include <string>

#include "s3/core/physics/skeleton_body.hpp"
#include "s3/data/collections.hpp"
#include "s3/data/vfs.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/skeleton_controller.hpp"
#include "s3/game/physics/wipeout_states/ragdoll/settings.hpp"

namespace s3::game::physics::wipeout_states::ragdoll {

struct RagdollSetup {
    settings::Settings settings{};

    static rs::Result<RagdollSetup, std::string> load(const sd::Collections& data, const sd::Vfs& vfs,
                                                      const std::string& assets, const std::string& bank_sha);

    Status request(skeleton_controller::SkeletonControllerState& controller, std::uint32_t requested,
                   ::s3::physics::skeleton_body::SkeletonBody& body,
                   ::s3::physics::skeleton_body::SkeletonJoints& joints,
                   ::s3::physics::skeleton_body::SkeletonCollisionMode& collision) const;

    ///Original SetUpNormal82BE7280: limits, normal table/group, observation
    ///reset, zero drags, authored animated mass/inertia/material, collision mode.
    ///Call at the original physical reset owner, never at WipeoutExit alone.
    void restore_normal(::s3::physics::skeleton_body::SkeletonBody& body,
                        ::s3::physics::skeleton_body::SkeletonJoints& joints,
                        ::s3::physics::skeleton_body::SkeletonCollisionMode& collision,
                        ::s3::physics::skeleton_body::SkeletonCollisionFeedback& feedback) const;
};

}  // namespace s3::game::physics::wipeout_states::ragdoll
