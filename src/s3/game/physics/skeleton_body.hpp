// Port of skate-game/src/physics/skeleton_body.rs.
//! Stock physical body construction. Initial mapped pose and spawn are real
//! actor/physics inputs supplied by the coordinator; no pose or COM fallback.
//
// Rust fns taking `asset_root: &Path` take `(const sd::Vfs& vfs, const std::string& asset_root)`.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "s3/core/animation/slice.hpp"
#include "s3/core/physics/rigid_body.hpp"
#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/core/physics/skeleton_body.hpp"
#include "s3/data/collections.hpp"
#include "s3/data/vfs.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/skeleton_body_collision_settings.hpp"
#include "s3/game/physics/skeleton_body_drive_settings.hpp"

namespace s3::game::physics::skeleton_body {

using sc::physics::skeleton_animation_record::AnimationPartTransform;

rs::Result<sc::physics::skeleton_body::SkeletonCollisionMode, std::string> load_collision(
    const sd::Vfs& vfs, const std::string& asset_root, const sd::Collections& data, std::string_view bank_sha,
    bool cull_all_self_pairs);

rs::Result<sc::physics::skeleton_body::SkeletonCollisionFeedback, std::string> load_feedback(
    const sd::Collections& data, sc::physics::skeleton_body::SkeletonCollisionSettings settings);

/// Build the complete original26-part drive owner using the already-resolved
/// joint topology. The initial bone matrices precede physical-volume mapping.
rs::Result<sc::physics::skeleton_body::SkeletonDrives, std::string> load_drives(
    const sd::Vfs& vfs, const std::string& asset_root, const sd::Collections& data,
    std::string_view animation_bank_sha256, sc::animation::Slice<const AnimationPartTransform> initial_hierarchy,
    const std::array<std::size_t, 24>& bone_indices, const std::array<AnimationPartTransform, 24>& initial_mapped,
    const sc::physics::skeleton_body::SkeletonJoints& joints, AnimationPartTransform animation_to_world,
    AnimationPartTransform spawn, sc::physics::rigid_body::RetailSimulationStep simulation);

/// SkeletonData::Init82BD6C40 / closest physical ancestor82BD3720.
/// Both the rig pose and hierarchy are the actor's evaluated initial RIG_TPOSE.
rs::Result<sc::physics::skeleton_body::SkeletonJoints, std::string> load_joints(
    const sd::Vfs& vfs, const std::string& asset_root, const sd::Collections& data,
    std::string_view animation_bank_sha256, sc::animation::Slice<const AnimationPartTransform> initial_hierarchy,
    sc::animation::Slice<const std::int32_t> hierarchy_parents, const std::array<std::size_t, 24>& bone_indices);

/// `hat_collection: Option<&str>` -> std::optional<std::string_view>.
rs::Result<sc::physics::skeleton_body::SkeletonBody, std::string> load(
    const sd::Vfs& vfs, const std::string& asset_root, const sd::Collections& data,
    std::string_view animation_bank_sha256, const std::array<AnimationPartTransform, 24>& initial_mapped_pose,
    AnimationPartTransform spawn, sc::physics::rigid_body::RetailSimulationStep simulation,
    std::optional<std::string_view> hat_collection);

}  // namespace s3::game::physics::skeleton_body
