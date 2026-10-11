// Port of skate-game/src/physics/skeleton_body_collision_settings.rs
// (mounted as `skeleton_body::collision_settings` via #[path]).
//! Original82BE7860/82BE7280 and exact locations in stock physics_skeleton XML.
#pragma once
#include <string>
#include <string_view>

#include "s3/core/physics/skeleton_body.hpp"
#include "s3/data/collections.hpp"
#include "s3/data/vfs.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::skeleton_body::collision_settings {

/// pub(super)
rs::Result<sc::physics::skeleton_body::SkeletonCollisionSettings, std::string> load(const sd::Vfs& vfs,
                                                                                     const std::string& asset_root,
                                                                                     const sd::Collections& data,
                                                                                     std::string_view bank_sha);

/// GlobalAttributeCollection+268 binds original low hashes7AA75DF9/2D7D2152
/// at8289F634: physics_collision/default. Stock XML baseC6AE0 independently
/// resolves every layout offset consumed by82BD4A30 and82BD6038.
rs::Result<sc::physics::skeleton_body::SkeletonFeedbackSettings, std::string> feedback(
    const sd::Collections& data, sc::physics::skeleton_body::SkeletonCollisionSettings body);

}  // namespace s3::game::physics::skeleton_body::collision_settings
