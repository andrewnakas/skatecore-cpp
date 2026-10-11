// Port of skate-core/src/physics/skeleton_general.rs.
// Original Skeleton::GeneralUpdate82BDCA38 and discontinuity reset82BE2798.
// This operates on the same physical bodies later borrowed by the solver.
#pragma once
#include <array>

#include "s3/core/physics/rigid_body.hpp"
#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/core/physics/skeleton_body.hpp"

namespace s3::physics::skeleton_general {

using Transform = skeleton_animation_record::AnimationPartTransform;
using rigid_body::RetailSimulationStep;
using skeleton_animation_record::SkeletonAnimationRecord;
using skeleton_body::SkeletonBody;

///82BDCA50..CAB8: root-position derivative, two reciprocal refinements,
///then history replacement. It is distinct from animation trajectory velocity.
std::array<float, 4> root_velocity(std::array<float, 4>& previous, std::array<float, 4> current, float dt);

///82BDCB50..CC18 uses its fixed-step literal, not the caller timestep.
///The resulting displacement enters ApplyDisplacementToPart82BE7788, which
///updates force accumulators and cooldown, rather than body translations.
void apply_gravity_displacement(SkeletonBody& body, float processed_gravity);

struct ResetObservation {
    ///Skeleton16208, current adjusted animation hips translation.
    std::array<float, 4> local_hips{};
    ///Skeleton16144 and16160 receive this value;16176 is zeroed.
    std::array<float, 4> world_hips{};
};

///82BE2798: reset physical pose to the current mapped animation after a
///target discontinuity. Reset rates exactly here; ordinary postphysics pose
///correction must preserve rates and uses SkeletonBody::set_part_transform.
ResetObservation reset_to_animation(SkeletonBody& body, SkeletonAnimationRecord& animation,
                                    const std::array<Transform, 24>& drive_pose, const Transform& animation_to_world,
                                    Transform com_frame, RetailSimulationStep simulation);

}  // namespace s3::physics::skeleton_general
