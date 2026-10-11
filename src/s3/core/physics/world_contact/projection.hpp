// Port of skate-core/src/physics/world_contact/projection.rs.
#pragma once
#include <utility>

#include "s3/core/physics/world_contact/arithmetic.hpp"

namespace s3::physics::world_contact::projection {

/// Verified GP dispatch IDs from mapped TU3 table 82FD5870. Cylinder (5) and
/// invalid (0) are outside the board primitive set and have no fallback here.
enum class PrimitiveKind {
    Sphere,
    Capsule,
    Triangle,
    Box,
};

/// Full single-direction GP interval callbacks: sphere82ADD800,
/// capsule82AD99C8, triangle82ADE3B8, box82AD8508. Output is the native48-byte
/// interval; the last16bytes are untouched. Radius/padding is handled by caller.
void project_direction(const std::array<std::uint32_t, 48>& gp, PrimitiveKind kind,
                       std::array<std::uint32_t, 4> direction, std::array<std::uint32_t, 12>& output);

/// Full batch GP callbacks: sphere82ADD820, capsule82AD9A88,
/// triangle82ADE400, box82AD86B0. Box batch scales the axes *before* dot products;
/// its single callback scales the dot products instead. Preserve that difference.
/// (Rust slices: `directions[..count]`, `output` with at least `count` entries.)
void project_directions(const std::array<std::uint32_t, 48>& gp, PrimitiveKind kind,
                        const std::array<std::uint32_t, 4>* directions, std::size_t count,
                        std::array<std::uint32_t, 12>* output, std::size_t output_len);

/// Complete generic82ACF070 plus recovered GP batch callbacks for board shapes.
/// Specialized triangle/box82ACF950 and box/box82ACF448 dispatch entries must
/// still be selected by the caller; this function is only the generic entry.
/// Returns (broadcast separation, oriented four-lane axis).
std::pair<std::array<std::uint32_t, 4>, std::array<std::uint32_t, 4>> best_separating_direction(
    const std::array<std::uint32_t, 48>& a, PrimitiveKind a_kind, const std::array<std::uint32_t, 48>& b,
    PrimitiveKind b_kind);

}  // namespace s3::physics::world_contact::projection
