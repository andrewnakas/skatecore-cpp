// Port of skate-core/src/physics/foot_physical_output.rs.
// Full original SkeletonIK::FillPhysOut_FeetDeckInteraction82BF22A0.
// Physical toe positions and derivatives are observed in the physical board's
// frame. They are distinct from animation targets and contact-query validity.
#pragma once
#include <array>

#include "s3/core/physics/skeleton_body.hpp"

namespace s3::physics::foot_physical_output {

using skeleton_body::SkeletonPhysicalRecord;

struct FootPhysicalSettings {
    float deck_half_width = 0.0f;
    float deck_total_half_length = 0.0f;
    /// Original physics_skeletonik layout64 FootOnDeckPadding.
    std::array<float, 4> padding{};
};

struct FootPhysicalOutput {
    /// PhysOutSkeleton192/208: change in local physical toe positions / dt.
    std::array<std::array<float, 4>, 2> local_velocity{};
    /// PhysOutSkeleton224/240: physical record velocities15/19, copied intact.
    std::array<std::array<float, 4>, 2> world_velocity{};
    /// PhysOutSkeleton600/601. Strict XYZ containment, with authored padding.
    std::array<bool, 2> within_deck_box{};
};

struct FootPhysicalState {
    /// IK3136/3152. Reset82BEDB5C/60 writes both vectors to zero.
    std::array<std::array<float, 4>, 2> previous_local_toes{};

    void reset();
    FootPhysicalOutput update(const SkeletonPhysicalRecord& record, float dt, FootPhysicalSettings settings);
};

}  // namespace s3::physics::foot_physical_output
