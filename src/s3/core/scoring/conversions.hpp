// Port of skate-core/src/scoring/conversions.rs.
// Scorable descriptor conversion links at820862A8, stride24.
#pragma once
#include <array>
#include <cstdint>
#include <utility>

namespace s3::scoring::conversions {

/// Rust tuple `(i32, i32)`.
using Link = std::pair<std::int32_t, std::int32_t>;

extern const std::array<Link, 332> LINKS;

}  // namespace s3::scoring::conversions
