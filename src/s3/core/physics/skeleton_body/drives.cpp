// Port of skate-core/src/physics/skeleton_body/drives.rs.
#include "s3/core/physics/board_pose.hpp"
#include "s3/core/physics/skeleton_body.hpp"
#include "s3/core/physics/skeleton_root.hpp"

namespace s3::physics::skeleton_body::drives {

using board_pose::orthonormalize_part_basis;
using drive_solver::build_drive_rows;
using drive_solver::RetailDriveBodyState;
using rigid_body::pack_world_inverse_inertia;
using skeleton_animation_record::compose_affine;
using skeleton_root::inverse_rigid;
using targets::HARD_STRENGTH;
using targets::HARD_VELOCITY;
using targets::hard;
using targets::TARGET_PARTS;

namespace {
RetailDriveBodyState drive_body(const BodySnapshot& body, std::size_t reaction_index) {
    const auto& rates = body.rates;
    RetailDriveBodyState out;
    out.reaction_index = reaction_index;
    out.state = body.state_flags;
    out.orientation = rates.orientation;
    out.basis = rates.basis;
    out.center_of_mass = rates.position;
    out.linear_velocity = rates.linear_velocity;
    out.angular_velocity = rates.angular_velocity;
    out.force_acceleration = rates.force_acceleration;
    out.torque_acceleration = rates.torque_acceleration;
    out.inverse_mass = body.inertia.inverse_mass;
    out.world_inverse_inertia = pack_world_inverse_inertia(rates.world_inverse_inertia);
    return out;
}
}  // namespace

rs::Result<SkeletonDrives, const char*> SkeletonDrives::new_(
    const std::array<AnimationPartTransform, ANIMATION_PART_COUNT>& initial_bones,
    const std::array<AnimationPartTransform, ANIMATION_PART_COUNT>& initial_mapped,
    const std::array<std::optional<std::size_t>, ANIMATION_PART_COUNT>& parents,
    AnimationPartTransform animation_to_world, AnimationPartTransform spawn, RetailSimulationStep simulation,
    SkeletonDriveSettings settings) {
    bool invalid = parents[0].has_value() || parents[23].has_value();
    for (const auto& p : parents) {
        if (p && *p >= ANIMATION_PART_COUNT) invalid = true;
    }
    for (std::size_t i = 1; i < 23; ++i) {
        if (!parents[i]) invalid = true;
    }
    if (invalid) {
        return rs::make_err("Skeleton drives require22 bone pairs and two roots");
    }
    std::array<AnimationPartTransform, ANIMATION_PART_COUNT> inverses{};
    for (std::size_t i = 0; i < ANIMATION_PART_COUNT; ++i) inverses[i] = inverse_rigid(initial_bones[i]);
    SkeletonDrives out;
    for (std::size_t part = 0; part < ANIMATION_PART_COUNT; ++part) {
        if (!parents[part]) {
            out.bones[part] = std::nullopt;
            continue;
        }
        const std::size_t parent = *parents[part];
        const std::size_t channel_parents[2] = {parent, 23};
        std::array<RetailDriveFrames, 2> frames{};
        for (std::size_t c = 0; c < 2; ++c) {
            frames[c] = bone_drive_frames(initial_bones[part], inverses[part], inverses[channel_parents[c]]);
        }
        BoneDriveDynamics dynamics = BoneDriveDynamics::default_();
        // Init82BED180: LOCAL mode2 followed by LOCAL_ROOT mode0.
        dynamics.mode = 2;
        dynamics.enable(0, 1.0f, settings.bone);
        dynamics.mode = 0;
        dynamics.enable(1, 1.0f, settings.bone);
        out.bones[part] = BoneDrives{{parent, 23}, {true, true}, frames, dynamics};
    }
    out.targets = SkeletonTargets::new_(simulation);
    const AnimationPartTransform frame = initial_mapped[23];
    std::array<std::uint32_t, 16> w{};
    for (std::size_t i = 0; i < 16; ++i) w[i] = rs::to_bits(frame[i / 4][i % 4]);
    const auto normalized = orthonormalize_part_basis(w);
    AnimationPartTransform hips{};
    for (std::size_t i = 0; i < 4; ++i)
        for (std::size_t j = 0; j < 4; ++j) hips[i][j] = rs::from_bits(normalized[i * 4 + j]);
    out.targets.reset(compose_affine(animation_to_world, hips), spawn);
    out.settings = settings;
    return out;
}

void SkeletonDrives::update(const std::array<AnimationPartTransform, ANIMATION_PART_COUNT>& pose, bool partial,
                            float collision_weight) {
    std::array<AnimationPartTransform, ANIMATION_PART_COUNT> inverses{};
    for (std::size_t i = 0; i < ANIMATION_PART_COUNT; ++i) inverses[i] = inverse_rigid(pose[i]);
    const SkeletonDriveSettings s = settings;
    const std::array<float, 2> base = s.enabled ? s.strength : std::array<float, 2>{0.0f, 0.0f};
    targets.dynamics[0].linear = hard(HARD_VELOCITY, HARD_STRENGTH);
    for (std::size_t part = 1; part < 23; ++part) {
        if (!bones[part]) {
            continue;
        }
        BoneDrives& bone = *bones[part];
        const bool selected = !partial || (part >= 1 && part <= 5) || (part >= 7 && part <= 9);
        std::array<float, 2> strengths{};
        if (!(collision_weight >= 1.0f) && selected) {
            bone.dynamics.mode = 3;
            for (std::size_t channel = 0; channel < 2; ++channel) {
                const float c = s.collision_strength[part][channel];
                //82BEB6F0/F4 are fmadds, followed by scalar fmuls.
                strengths[channel] = rs::mul_add(1.0f - c, collision_weight, c) * s.strength[channel];
            }
        } else {
            bone.dynamics.mode = 0;
            strengths = base;
        }
        bone.dynamics.strengths = strengths;
        for (std::size_t channel = 0; channel < 2; ++channel) {
            if (!bone.active[channel]) {
                continue;
            }
            bone.frames[channel] = bone_drive_frames(pose[part], inverses[part], inverses[bone.parent[channel]]);
            bone.dynamics.enable(channel, strengths[channel], s.bone);
        }
    }
}

SkeletonDriveBatch SkeletonDrives::build(const std::array<BodySnapshot, PART_COUNT>& bodies,
                                         std::size_t reaction_base, std::size_t target_reaction_base,
                                         float time_step) {
    SkeletonDriveBatch batch;
    batch.rows.reserve(48);
    batch.identities.reserve(48);
    batch.spy.reserve(48);
    for (std::size_t target = 0; target < TARGET_COUNT; ++target) {
        targets.frames[target] = prepare_bone_drive_frames(targets.frames[target]);
        const std::size_t part = TARGET_PARTS[target];
        const BodySnapshot a = bodies[part];
        const BodySnapshot b = targets.bodies[target];
        if (((a.state_flags | b.state_flags) & 4u) != 0) {
            batch.rows.push_back(build_drive_rows(drive_body(a, reaction_base + part),
                                                  drive_body(b, target_reaction_base + target),
                                                  targets.frames[target], targets.dynamics[target], time_step));
            batch.identities.push_back(SkeletonDriveIdentity::Target(target));
            batch.spy.push_back(target >= 2);
        }
    }
    for (std::size_t part = 1; part < 23; ++part) {
        if (!bones[part]) {
            continue;
        }
        BoneDrives& bone = *bones[part];
        for (std::size_t channel = 0; channel < 2; ++channel) {
            if (!bone.active[channel]) {
                continue;
            }
            bone.frames[channel] = prepare_bone_drive_frames(bone.frames[channel]);
            const std::size_t parent = bone.parent[channel];
            const BodySnapshot a = bodies[part];
            const BodySnapshot b = bodies[parent];
            if (((a.state_flags | b.state_flags) & 4u) != 0) {
                batch.rows.push_back(build_drive_rows(drive_body(a, reaction_base + part),
                                                      drive_body(b, reaction_base + parent), bone.frames[channel],
                                                      bone.dynamics.channels[channel], time_step));
                batch.identities.push_back(SkeletonDriveIdentity::Bone(part, channel));
                batch.spy.push_back(true);
            }
        }
    }
    return batch;
}

}  // namespace s3::physics::skeleton_body::drives
