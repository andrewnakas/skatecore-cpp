// Port of skate-game/src/physics/wipeout_states/ragdoll.rs
//! Physical-owner adapter for selector82D913C0 requests8..11 and82BE7280.
#include "s3/game/physics/wipeout_states/ragdoll.hpp"

namespace s3::game::physics::wipeout_states::ragdoll {

rs::Result<RagdollSetup, std::string> RagdollSetup::load(const sd::Collections& data, const sd::Vfs& vfs,
                                                         const std::string& assets, const std::string& bank_sha) {
    S3G_TRY(settings::Settings s, settings::Settings::load(data, vfs, assets, bank_sha));
    return RagdollSetup{s};
}

Status RagdollSetup::request(skeleton_controller::SkeletonControllerState& controller, std::uint32_t requested,
                             ::s3::physics::skeleton_body::SkeletonBody& body,
                             ::s3::physics::skeleton_body::SkeletonJoints& joints,
                             ::s3::physics::skeleton_body::SkeletonCollisionMode& collision) const {
    const auto selected = controller.select_request(requested);
    if (!selected) return ok();
    const std::uint32_t mode = *selected;
    if (!(mode >= 7 && mode <= 10)) {
        auto r = collision.select_driven(mode);
        if (r.is_err()) return rs::make_err(std::string(r.unwrap_err()));
        return ok();
    }
    //Request11 calls82BE3350(true), selecting normal limits while the
    //body still receives ragdoll masses, materials, drag and group6.
    const auto& limits = mode == 10 ? settings.normal_limits : settings.ragdoll_limits;
    const std::size_t n = std::min(joints.records.size(), limits.size());
    for (std::size_t j = 0; j < n; ++j) {
        for (std::size_t k = 0; k < 4; ++k) joints.records[j].parameters.words[10 + k] = limits[j][k];
    }
    collision.apply_ragdoll_properties(body, settings.inverse_mass, settings.inverse_inertia, settings.drag,
                                       settings.materials);
    collision.finish_ragdoll_request(mode);
    return ok();
}

void RagdollSetup::restore_normal(::s3::physics::skeleton_body::SkeletonBody& body,
                                  ::s3::physics::skeleton_body::SkeletonJoints& joints,
                                  ::s3::physics::skeleton_body::SkeletonCollisionMode& collision,
                                  ::s3::physics::skeleton_body::SkeletonCollisionFeedback& feedback) const {
    const std::size_t n = std::min(joints.records.size(), settings.normal_limits.size());
    for (std::size_t j = 0; j < n; ++j) {
        for (std::size_t k = 0; k < 4; ++k) joints.records[j].parameters.words[10 + k] = settings.normal_limits[j][k];
    }
    feedback.set_up_normal();
    collision.restore_normal_properties(body);
}

}  // namespace s3::game::physics::wipeout_states::ragdoll
