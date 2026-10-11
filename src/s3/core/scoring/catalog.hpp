// Port of skate-core/src/scoring/catalog.rs.
// TU3 EScorableID enum metadata, table820862A8 (332 x24 bytes).
// Numeric class/type relationships and native identifier spellings only.
// Points, localization, timers and UI assets are loaded from owned data.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <tuple>

#include "s3/core/scoring.hpp"

namespace s3::scoring::catalog {

/// Rust tuple `(&str, u32, usize)`: (identifier, class, score_type).
using Identifier = std::tuple<std::string_view, std::uint32_t, std::size_t>;

extern const std::array<Identifier, 332> IDENTIFIERS;

/// A scorable's class and score type as the fixed table holds them.
///
/// EndAirTrick82DA6260 indexes table820862A8 by bare id (`rlwinm r11,r4,1,0,30` /
/// `addi r11,r10,25256` / `lwzx`) and never consults authored data, so a collector can
/// credit a scorable the vault has no record for. The metric scorables are exactly
/// that case: 129..=133 (`air_horizontal_distance`, `air_height_to_peak`,
/// `air_total_height_gain`, `air_player_spin`, `air_player_flip`), 237 (`air_metric`)
/// and 253 (`generic_metric`) carry no `Hash_6918469984A8C596` record in the owner's
/// vault, because their reward is a curve value rather than authored points.
/// Resolving them through [`crate::scoring::catalog`] instead of the authored table is
/// what keeps 82DA8550's landing credit from being dropped.
std::optional<Scorable> metadata(std::size_t id);

}  // namespace s3::scoring::catalog
