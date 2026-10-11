// Port of skate-core/src/physics/skeleton_body/record.rs (included via skeleton_body.hpp).
// SkeletonState's physical record: TU3 82BEBD28 and 82BEC1E8.
#pragma once
#include <array>

#include "s3/core/physics/skeleton_animation_record.hpp"

namespace s3::physics::skeleton_body::record {

using skeleton_animation_record::AnimationPartTransform;

struct SkeletonPhysicalRecord {
    /// SkeletonState1552: actual part frames, including the mass-frame inverse.
    std::array<AnimationPartTransform, PART_COUNT> pose = [] {
        std::array<AnimationPartTransform, PART_COUNT> a{};
        a.fill(skeleton_animation_record::IDENTITY);
        return a;
    }();
    /// SkeletonState3216: previous positions used by the fixed-step derivative.
    std::array<std::array<float, 4>, PART_COUNT> positions{};
    /// SkeletonState3632: pose-derived velocities, not rigid body rates.
    std::array<std::array<float, 4>, PART_COUNT> velocities{};
    /// SkeletonState4048: difference from the preceding derived velocity.
    std::array<std::array<float, 4>, PART_COUNT> velocity_changes{};
    /// SkeletonState4464/4480; only the first 24 weighted parts contribute.
    std::array<float, 4> centre_of_mass{};
    std::array<float, 4> centre_of_mass_velocity{};
    float timestep = 0.0f;

    /// Default: Constructor82BEB8C8 initializes all26 physical transforms and history.
    /// (The member initializers above equal Rust's `Default`.)
    static SkeletonPhysicalRecord default_() { return SkeletonPhysicalRecord{}; }

    /// Physical subset of Reset82BEBBB0. The native reset preserves physical
    /// COM, velocity changes and timestep; other animation/error owners reset
    /// their own fields at this same phase.
    void reset(const std::array<AnimationPartTransform, PART_COUNT>& parts);

    /// The caller supplies the actual GetPartTransform results. Native part0
    /// uses the explicit board-frame argument; all other parts come from the
    /// skeleton assembly. Keep this distinction when the board drives a proxy.
    void update(const std::array<AnimationPartTransform, PART_COUNT>& parts,
                AnimationPartTransform board_frame,
                const std::array<float, ANIMATION_PART_COUNT>& fractional);
};

}  // namespace s3::physics::skeleton_body::record

namespace s3::physics::skeleton_body {
using record::SkeletonPhysicalRecord;
}  // namespace s3::physics::skeleton_body
