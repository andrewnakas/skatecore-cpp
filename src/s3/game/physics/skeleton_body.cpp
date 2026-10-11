// Port of skate-game/src/physics/skeleton_body.rs.
#include "s3/game/physics/skeleton_body.hpp"

#include <algorithm>
#include <vector>

#include "s3/core/math.hpp"
#include "s3/data/physics_skeleton.hpp"

namespace s3::game::physics::skeleton_body {

namespace sb = ::s3::physics::skeleton_body;
using sc::math::Vector3;

namespace {
rs::Result<sd::PhysicsSkeleton, std::string> load_skeleton(const sd::Vfs& vfs, const std::string& asset_root,
                                                           std::string_view bank_sha) {
    S3G_TRY(auto skeleton,
            sd::PhysicsSkeleton::load(vfs, sd::join_path(asset_root, "private/stock/physics-skeletons.json"),
                                      std::string(bank_sha), "PHYS_TPOSE"));
    return skeleton;
}
}  // namespace

rs::Result<sb::SkeletonCollisionMode, std::string> load_collision(const sd::Vfs& vfs, const std::string& asset_root,
                                                                  const sd::Collections& data,
                                                                  std::string_view bank_sha,
                                                                  bool cull_all_self_pairs) {
    S3G_TRY(auto settings, collision_settings::load(vfs, asset_root, data, bank_sha));
    return sb::SkeletonCollisionMode::new_normal(settings, cull_all_self_pairs);
}

rs::Result<sb::SkeletonCollisionFeedback, std::string> load_feedback(const sd::Collections& data,
                                                                     sb::SkeletonCollisionSettings settings) {
    S3G_TRY(auto feedback_settings, collision_settings::feedback(data, settings));
    return sb::SkeletonCollisionFeedback::new_(feedback_settings);
}

rs::Result<sb::SkeletonDrives, std::string> load_drives(
    const sd::Vfs& vfs, const std::string& asset_root, const sd::Collections& data,
    std::string_view animation_bank_sha256, sc::animation::Slice<const AnimationPartTransform> initial_hierarchy,
    const std::array<std::size_t, 24>& bone_indices, const std::array<AnimationPartTransform, 24>& initial_mapped,
    const sb::SkeletonJoints& joints, AnimationPartTransform animation_to_world, AnimationPartTransform spawn,
    sc::physics::rigid_body::RetailSimulationStep simulation) {
    if (std::any_of(bone_indices.begin(), bone_indices.end(),
                    [&](std::size_t i) { return i >= initial_hierarchy.size(); }))
        return rs::make_err(std::string("Skeleton drive initial hierarchy has missing bones"));
    S3G_TRY(auto skeleton, load_skeleton(vfs, asset_root, animation_bank_sha256));
    if (skeleton.bones.size() != 24) return rs::make_err(std::string("Skeleton drives require24 bone records"));
    std::array<std::optional<std::size_t>, 24> parents{};
    for (const auto& joint : joints.records) parents[joint.child] = joint.parent;
    std::array<AnimationPartTransform, 24> initial_bones{};
    for (std::size_t i = 0; i < 24; ++i) initial_bones[i] = initial_hierarchy[bone_indices[i]];
    std::array<std::array<float, 2>, 24> collision_strength{};
    //82BE7B98..BAC builds057EDDFA341F9492=CollisionDriveScalar.
    S3G_TRY(const float scalar, data.float_("physics_skeleton_drives", "default", "CollisionDriveScalar"));
    for (std::size_t part = 1; part < 24; ++part) {
        const std::string name = "PART_" + skeleton.bones[part].name;
        S3G_TRY(auto words, data.words<9>("physics_skeleton_drives", "default", name));
        collision_strength[part] = {rs::from_bits(words[5]) * scalar, rs::from_bits(words[6]) * scalar};
    }
    sb::SkeletonDriveSettings settings;
    S3G_TRY(settings.bone, drive_settings::load(data));
    S3G_TRY(settings.enabled, data.boolean("animation", "default", "Use_Drives"));
    S3G_TRY(settings.strength[0], data.float_("animation", "default", "DriveStrengthLocal"));
    S3G_TRY(settings.strength[1], data.float_("animation", "default", "DriveStrengthRootLocal"));
    settings.collision_strength = collision_strength;
    auto drives = sb::SkeletonDrives::new_(initial_bones, initial_mapped, parents, animation_to_world, spawn,
                                           simulation, settings);
    if (drives.is_err()) return rs::make_err(std::string(drives.unwrap_err()));
    return std::move(drives.unwrap());
}

rs::Result<sb::SkeletonJoints, std::string> load_joints(
    const sd::Vfs& vfs, const std::string& asset_root, const sd::Collections& data,
    std::string_view animation_bank_sha256, sc::animation::Slice<const AnimationPartTransform> initial_hierarchy,
    sc::animation::Slice<const std::int32_t> hierarchy_parents, const std::array<std::size_t, 24>& bone_indices) {
    if (hierarchy_parents.size() != initial_hierarchy.size() ||
        std::any_of(bone_indices.begin(), bone_indices.end(),
                    [&](std::size_t i) { return i >= initial_hierarchy.size(); }))
        return rs::make_err(std::string("Physical joint hierarchy differs from initial rig pose"));
    S3G_TRY(auto skeleton, load_skeleton(vfs, asset_root, animation_bank_sha256));
    if (skeleton.bones.size() != 24) return rs::make_err(std::string("Physical joints require24 bone records"));
    std::array<AnimationPartTransform, 24> initial_bones{};
    for (std::size_t i = 0; i < 24; ++i) initial_bones[i] = initial_hierarchy[bone_indices[i]];
    std::array<std::optional<std::size_t>, 24> parents{};
    for (std::size_t child = 0; child < 24; ++child) {
        std::int32_t parent = hierarchy_parents[bone_indices[child]];
        std::size_t visited = 0;
        while (parent != -1) {
            if (parent < 0) return rs::make_err(std::string("Invalid animation parent index"));
            const std::size_t index = static_cast<std::size_t>(parent);
            if (index >= hierarchy_parents.size() || visited >= hierarchy_parents.size())
                return rs::make_err(std::string("Invalid or cyclic animation hierarchy"));
            const auto found = std::find(bone_indices.begin(), bone_indices.end(), index);
            if (found != bone_indices.end()) {
                parents[child] = static_cast<std::size_t>(found - bone_indices.begin());
                break;
            }
            parent = hierarchy_parents[index];
            visited += 1;
        }
    }
    std::array<sb::JointBone, 24> bones{};
    for (std::size_t i = 0; i < 24; ++i) {
        const auto& bone = skeleton.bones[i];
        const auto t = bone.translation();
        sb::JointBone jb;
        for (std::size_t lane = 0; lane < 4; ++lane) {
            jb.parent_orientation[lane] = rs::from_bits(bone.words[lane]);
            jb.joint_orientation[lane] = rs::from_bits(bone.words[4 + lane]);
        }
        jb.volume_frame =
            sc::physics::skeleton_animation_record::physics_bone_frame(bone.rotation(), {t[0], t[1], t[2], 0.0f});
        jb.swing_limit = rs::from_bits(bone.words[21]);
        jb.twist_limit = rs::from_bits(bone.words[20]);
        bones[i] = jb;
    }
    // InitAttribData stores these in ascending physical child order1..22.
    const char* names[22] = {
        "JOINT_NECK_NECK1",          "JOINT_SPINE3_NECK",          "JOINT_LEFT_FOREARM_HAND",
        "JOINT_LEFT_ARM_FOREARM",    "JOINT_LEFT_SHOULDER_ARM",    "JOINT_SPINE3_LEFT_SHOULDER",
        "JOINT_RIGHT_FOREARM_HAND",  "JOINT_RIGHT_ARM_FOREARM",    "JOINT_RIGHT_SHOULDER_ARM",
        "JOINT_SPINE3_RIGHT_SHOULDER", "JOINT_SPINE2_SPINE3",      "JOINT_SPINE1_SPINE2",
        "JOINT_SPINE_SPINE1",        "JOINT_HIPS_SPINE",           "JOINT_LEFT_FOOT_TOE_BASE",
        "JOINT_LEFT_LEG_FOOT",       "JOINT_LEFT_UPLEG_LEG",       "JOINT_HIPS_LEFT_LEG",
        "JOINT_RIGHT_FOOT_TOE_BASE", "JOINT_RIGHT_LEG_FOOT",       "JOINT_RIGHT_UPLEG_LEG",
        "JOINT_HIPS_RIGHT_LEG",
    };
    std::array<sb::JointSettings, 22> settings{};
    for (std::size_t i = 0; i < 22; ++i) {
        S3G_TRY(auto words, data.words<5>("physics_skeleton_joints", "default", names[i]));
        sb::JointSettings js;
        js.ball_joint = (words[0] >> 24) != 0;
        js.swing_angle = rs::from_bits(words[1]);
        js.twist_angle = rs::from_bits(words[2]);
        js.swing_ragdoll = rs::from_bits(words[3]);
        js.twist_ragdoll = rs::from_bits(words[4]);
        settings[i] = js;
    }
    sb::SkeletonJointSettings global;
    {
        S3G_TRY(auto w, data.words<4>("physics_skeleton", "default", "DisplacementLimit"));
        global.displacement_limit = rs::from_bits(w);
    }
    S3G_TRY(global.twist_displacement_limit, data.float_("physics_skeleton", "default", "TwistDisplacementLimit"));
    S3G_TRY(global.swing_displacement_limit, data.float_("physics_skeleton", "default", "SwingDisplacementLimit"));
    S3G_TRY(global.enforce_swing_free, data.boolean("physics_skeleton_joints", "default", "EnforceSwingFree"));
    S3G_TRY(global.enforce_twist_free, data.boolean("physics_skeleton_joints", "default", "EnforceTwistFree"));
    auto joints = sb::SkeletonJoints::new_(initial_bones, parents, bones, settings, global);
    if (joints.is_err()) return rs::make_err(std::string(joints.unwrap_err()));
    return std::move(joints.unwrap());
}

rs::Result<sb::SkeletonBody, std::string> load(const sd::Vfs& vfs, const std::string& asset_root,
                                               const sd::Collections& data, std::string_view animation_bank_sha256,
                                               const std::array<AnimationPartTransform, 24>& initial_mapped_pose,
                                               AnimationPartTransform spawn,
                                               sc::physics::rigid_body::RetailSimulationStep simulation,
                                               std::optional<std::string_view> hat_collection) {
    S3G_TRY(auto skeleton, load_skeleton(vfs, asset_root, animation_bank_sha256));
    if (skeleton.bones.size() != 24) return rs::make_err(std::string("Physical skeleton requires24 bone records"));
    std::array<Vector3, 24> sizes{};
    std::array<sb::BoneSettings, 24> bones{};
    for (std::size_t i = 0; i < skeleton.bones.size(); ++i) {
        const auto& bone = skeleton.bones[i];
        const auto size = bone.size();
        sizes[i] = Vector3{size[0], size[1], size[2]};
        S3G_TRY(auto w, data.words<6>("physics_skeleton_bones", "default", "PART_" + bone.name));
        sb::BoneSettings b;
        b.mass_factor = rs::from_bits(w[0]);
        b.ragdoll_mass_factor = rs::from_bits(w[1]);
        b.has_collision = (w[2] >> 24) != 0;
        b.use_root_drive = ((w[2] >> 16) & 255u) != 0;
        b.volume_type = w[3];
        b.volume_scalar = rs::from_bits(w[4]);
        b.num_parents = w[5];
        bones[i] = b;
    }
    auto value = [&](const char* name) { return data.float_("physics_skeleton", "default", name); };
    sb::SkeletonBodySettings settings;
    S3G_TRY(settings.density, value("MassOfSkeleton"));
    S3G_TRY(settings.root_radius, value("SkateRootCapsuleRadius"));
    S3G_TRY(settings.root_half_length, value("SkateRootCapsuleLength"));
    S3G_TRY(settings.capsule_radius_scalar, data.float_("physics_skeleton_bones", "default", "CapsuleRadiusScalar"));
    S3G_TRY(settings.capsule_length_scalar, value("CapsuleLengthScalar"));
    S3G_TRY(settings.ragdoll_inverse_mass_factor, data.float_("physics_wipeout", "default", "RagdollInvMassFactor"));
    S3G_TRY(settings.inertia_multiply_type, data.integer("animation", "default", "InertiaMultiplyType"));
    S3G_TRY(settings.inertia_factor, data.float_("animation", "default", "InertiaMultFactor"));
    std::optional<sb::HatGeometry> hat;
    if (hat_collection) {
        const std::string key(*hat_collection);
        auto vector = [&](const char* name) -> rs::Result<Vector3, std::string> {
            S3G_TRY(auto words, data.words<4>("physics_hat", key, name));
            const auto w = rs::from_bits(words);
            return Vector3{w[0], w[1], w[2]};
        };
        S3G_TRY(const float radius, data.float_("physics_hat", key, "Hash_372DF5DB27AF1C79"));
        S3G_TRY(const float thickness, data.float_("physics_hat", key, "Thickness"));
        S3G_TRY(const Vector3 orientation, vector("OrientationOffset"));
        S3G_TRY(const Vector3 position, vector("PosOffset"));
        hat = sb::HatGeometry::from_offsets(radius, thickness, orientation, position);
    }
    auto definition = sb::SkeletonBodyDefinition::new_(sizes, bones, settings, hat);
    if (definition.is_err()) return rs::make_err(std::string(definition.unwrap_err()));
    return sb::SkeletonBody::new_(std::move(definition.unwrap()), initial_mapped_pose, spawn, simulation);
}

}  // namespace s3::game::physics::skeleton_body
