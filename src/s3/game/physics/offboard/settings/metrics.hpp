// Port of skate-game/src/physics/offboard/settings/metrics.rs
//!82D16680 uses first matching record, status4, length=frames/fps.
//!82D164F8 multiplies the authored end by that length, including end=-1.
#pragma once
#include <array>
#include <optional>
#include <string>

#include "s3/core/player/offboard/controller.hpp"
#include "s3/data/animation_metadata.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::offboard::settings::metrics {

using ClipMetric = sc::player::offboard::controller::ClipMetric;

inline constexpr std::array<const char*, 3> CLIPS{"NB_WALK_FWD_CYC", "NB_RUN_FWD_CYC", "NB_SPRINT_FWD_CYC"};

/// pub(super)
rs::Result<std::array<std::optional<ClipMetric>, 3>, std::string> load(const sd::AnimationMetadata& metadata);
/// pub(super)
rs::Result<std::optional<ClipMetric>, std::string> metric(const sd::ClipMetadata& clip);

}  // namespace s3::game::physics::offboard::settings::metrics
