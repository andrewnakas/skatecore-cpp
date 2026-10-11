// Port of skate-core/src/physics/skeleton_output/mod.rs.
// Original TU3 physical pose worker82DB95E8, submitted by82DB6698.
// Updates the existing animation buffers after the physical solve.
#pragma once
#include <array>
#include <cstddef>

#include "s3/core/animation/foot_ik/drive.hpp"
#include "s3/core/animation/foot_ik/transforms.hpp"
#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/core/physics/skeleton_output/board.hpp"
#include "s3/core/physics/skeleton_output/correction.hpp"
#include "s3/core/physics/skeleton_output/wobble.hpp"
#include "s3/core/physics/slice.hpp"

namespace s3::physics::skeleton_output {

using Transform = skeleton_animation_record::AnimationPartTransform;
using animation::foot_ik::drive::Geometry;

struct Input {
    /// Skeleton8016, all24 solved physical volume transforms in world space.
    const std::array<Transform, 24>& physical_parts;
    /// Skeleton11984, captured by82DB66C0 before submitting this worker.
    const Transform& world_to_animation;
    board::Input board;
};

/// Stock SkeletonData ids, nearest physical ancestors, and volume inverses.
struct SkeletonOutput {
    std::array<std::size_t, 24> bone_indices{};
    Geometry geometry;
    board::BoneIndices board_bones;
    board::Settings board_settings;

    /// Buffers must contain the evaluated stock animation for this same frame.
    /// Unrepresented bones retain their animation. Board locals are modified
    /// after the mapped globals; the worker does not rebuild the full hierarchy.
    rs::Result<rs::Unit, const char*> publish(const Input& input, Slice<Transform> globals,
                                              Slice<Transform> locals) const;
};

}  // namespace s3::physics::skeleton_output
