// Port of skate-core/src/physics/solver/joint_data.rs (module solver::joint::data).
// Decoded joint/drive geometry and shared reaction application.
//
// Record boundaries are four-float columns because native solver stores also
// update their fourth lanes. Physical vectors/impulses have three components;
// the fourth values are packed carry fields, not extra spatial dimensions.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

#include "s3/core/math.hpp"
#include "s3/core/physics/slice.hpp"

namespace s3::physics::solver::joint::data {

using math::Vector3;
using Column = std::array<float, 4>;

Column read_column(Slice<std::uint32_t> words, std::size_t byte_offset);
void write_column(Slice<std::uint32_t> words, std::size_t byte_offset, Column values);
inline Vector3 xyz(Column values) { return Vector3{values[0], values[1], values[2]}; }

/// Velocity-producing displacement corrections at reaction offsets 0/32.
/// Contact-only pose corrections at 16/48 are neither read nor overwritten.
struct ReactionState {
    Column linear{};
    Column angular{};

    static ReactionState decode(Slice<std::uint32_t> words);
};

/// `82AE1AE8` and `82AE3BC8` write these same response fields. Columns at
/// 64/96/128 and 80/112/144 project relative corrections into constraint axes;
/// 192..224 and 240..272 convert impulse changes back into world space.
struct ConstraintGeometry {
    Vector3 arm_a;
    Vector3 arm_b;
    std::array<Column, 3> linear_projection{};
    std::array<Column, 3> angular_projection{};
    // private in Rust
    std::array<Column, 3> linear_axes{};
    std::array<Column, 3> angular_axes{};
    std::array<Column, 3> inverse_inertia_a{};
    std::array<Column, 3> inverse_inertia_b{};

    static ConstraintGeometry decode(Slice<std::uint32_t> words);

    /// Both candidates must read this incoming snapshot before either impulse
    /// is applied: 82AE2C00..2CC4 (joint), 82AE2E90..2F80 (drive).
    std::pair<Vector3, Vector3> relative_corrections(const ReactionState& a, const ReactionState& b) const;

    /// 82AE2D24..2E40 / 82AE2FE0..30D4. One linear and one angular impulse
    /// change affect both bodies through their own arm and inverse inertia.
    void apply_impulse_changes(ReactionState& a, ReactionState& b, Vector3 linear_change,
                               Vector3 angular_change) const;
};

void publish_reactions(const ReactionState& a, const ReactionState& b, Slice<std::uint32_t> words_a,
                       Slice<std::uint32_t> words_b);

/// Matrix projection with the native X, Y, Z fused accumulation order. The
/// incoming accumulated impulse is the initial value for candidate projection.
Column project_correction(const std::array<Column, 3>& columns, Vector3 correction, Column initial);

}  // namespace s3::physics::solver::joint::data
