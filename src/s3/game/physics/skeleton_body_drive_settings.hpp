// Port of skate-game/src/physics/skeleton_body_drive_settings.rs
// (mounted as `skeleton_body::drive_settings` via #[path]).
//! Original BoneDrives constructor82BEC4A0/82BEC7B0 collection bindings.
//!82BEC50C..544 builds local=00132D1E84F0BC32;82BEC5B4..D4 builds
//! root=550B23128E0C3838. The12 transition keys preserve native offsets232..276.
#pragma once
#include <string>

#include "s3/core/physics/skeleton_body.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::skeleton_body::drive_settings {

/// pub(super)
rs::Result<sc::physics::skeleton_body::BoneDriveSettings, std::string> load(const sd::Collections& data);

}  // namespace s3::game::physics::skeleton_body::drive_settings
