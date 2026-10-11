// Port of skate-core/src/physics/skeleton_body/drives.rs (included via skeleton_body.hpp).
// Original SkeletonDrives82BEA670/82BEB488: two channels per physical bone
// and four independent static targets, all solved in the same island.
#pragma once
#include <array>
#include <cstddef>
#include <optional>
#include <vector>

#include "s3/core/physics/assembly.hpp"
#include "s3/core/physics/drive_frames.hpp"
#include "s3/core/physics/drive_solver.hpp"
#include "s3/core/physics/rigid_body.hpp"
#include "s3/core/physics/skeleton_animation_record.hpp"

namespace s3::physics::skeleton_body::drives {

using assembly::BodySnapshot;
using drive_solver::RetailDriveRows;
using physics::drive_frames::RetailDriveFrames;
using rigid_body::RetailSimulationStep;
using skeleton_animation_record::AnimationPartTransform;

struct SkeletonDriveSettings {
    BoneDriveSettings bone;
    bool enabled = false;
    std::array<float, 2> strength{};
    /// InitAttribData82BE7860 applies CollisionDriveScalar to each part's
    /// LOCAL and LOCAL_ROOT collision strengths before use in Update.
    std::array<std::array<float, 2>, ANIMATION_PART_COUNT> collision_strength{};
};

struct BoneDrives {
    std::array<std::size_t, 2> parent{};
    std::array<bool, 2> active{};
    std::array<RetailDriveFrames, 2> frames{};
    BoneDriveDynamics dynamics;
};

/// Identity stays parallel to eligible rows. Native m_spy requests force
/// observation; it does not disable a drive or remove it from the solve.
/// Rust `enum SkeletonDriveIdentity { Target(usize), Bone { part, channel } }`.
struct SkeletonDriveIdentity {
    enum class Kind { Target, Bone };
    Kind kind = Kind::Target;
    /// `Target(_0)`.
    std::size_t _0 = 0;
    /// `Bone { part, channel }`.
    std::size_t part = 0;
    std::size_t channel = 0;

    static SkeletonDriveIdentity Target(std::size_t index) {
        return SkeletonDriveIdentity{Kind::Target, index, 0, 0};
    }
    static SkeletonDriveIdentity Bone(std::size_t part, std::size_t channel) {
        return SkeletonDriveIdentity{Kind::Bone, 0, part, channel};
    }
};
inline bool operator==(const SkeletonDriveIdentity& a, const SkeletonDriveIdentity& b) {
    if (a.kind != b.kind) return false;
    return a.kind == SkeletonDriveIdentity::Kind::Target ? a._0 == b._0
                                                         : (a.part == b.part && a.channel == b.channel);
}
inline bool operator!=(const SkeletonDriveIdentity& a, const SkeletonDriveIdentity& b) { return !(a == b); }

struct SkeletonDriveBatch {
    std::vector<RetailDriveRows> rows;
    std::vector<SkeletonDriveIdentity> identities;
    std::vector<bool> spy;
};

struct SkeletonDrives {
    SkeletonTargets targets;
    std::array<std::optional<BoneDrives>, ANIMATION_PART_COUNT> bones{};
    SkeletonDriveSettings settings;

    static rs::Result<SkeletonDrives, const char*> new_(
        const std::array<AnimationPartTransform, ANIMATION_PART_COUNT>& initial_bones,
        const std::array<AnimationPartTransform, ANIMATION_PART_COUNT>& initial_mapped,
        const std::array<std::optional<std::size_t>, ANIMATION_PART_COUNT>& parents,
        AnimationPartTransform animation_to_world, AnimationPartTransform spawn, RetailSimulationStep simulation,
        SkeletonDriveSettings settings);

    ///82BEB488 consumes the24 IK-adjusted frames in animation coordinates.
    /// Collision weight comes from the actual SkeletonCollision state owner.
    void update(const std::array<AnimationPartTransform, ANIMATION_PART_COUNT>& pose, bool partial,
                float collision_weight);

    /// Native order: four targets, then LOCAL and LOCAL_ROOT for parts1..22.
    /// The caller appends rows to the board/contact/joint solve before any
    /// integration and borrows targets.bodies directly into that same step.
    SkeletonDriveBatch build(const std::array<BodySnapshot, PART_COUNT>& bodies, std::size_t reaction_base,
                             std::size_t target_reaction_base, float time_step);
};

}  // namespace s3::physics::skeleton_body::drives

namespace s3::physics::skeleton_body {
using drives::BoneDrives;
using drives::SkeletonDriveBatch;
using drives::SkeletonDriveIdentity;
using drives::SkeletonDrives;
using drives::SkeletonDriveSettings;
}  // namespace s3::physics::skeleton_body
