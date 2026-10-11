// Port of skate-game/src/physics/wipeout_states/ragdoll/settings.rs
//! Authored inputs to normal82BE58A8 and ragdoll82BE59A8/6D60.
#include "s3/game/physics/wipeout_states/ragdoll/settings.hpp"

#include "s3/core/trigonometry.hpp"
#include "s3/data/physics_skeleton.hpp"

namespace s3::game::physics::wipeout_states::ragdoll::settings {

using ::s3::physics::contact::RetailContactMaterial;

rs::Result<Settings, std::string> Settings::load(const sd::Collections& data, const sd::Vfs& vfs,
                                                 const std::string& assets, const std::string& bank_sha) {
    S3G_TRY(const sd::PhysicsSkeleton skeleton,
            sd::PhysicsSkeleton::load(vfs, sd::join_path(assets, "private/stock/physics-skeletons.json"), bank_sha,
                                      "PHYS_TPOSE"));
    if (skeleton.bones.size() != 24) return rs::make_err(std::string("Ragdoll requires24 physical bones"));
    const std::array<const char*, 22> names{
        "JOINT_NECK_NECK1",          "JOINT_SPINE3_NECK",         "JOINT_LEFT_FOREARM_HAND",
        "JOINT_LEFT_ARM_FOREARM",    "JOINT_LEFT_SHOULDER_ARM",   "JOINT_SPINE3_LEFT_SHOULDER",
        "JOINT_RIGHT_FOREARM_HAND",  "JOINT_RIGHT_ARM_FOREARM",   "JOINT_RIGHT_SHOULDER_ARM",
        "JOINT_SPINE3_RIGHT_SHOULDER", "JOINT_SPINE2_SPINE3",     "JOINT_SPINE1_SPINE2",
        "JOINT_SPINE_SPINE1",        "JOINT_HIPS_SPINE",          "JOINT_LEFT_FOOT_TOE_BASE",
        "JOINT_LEFT_LEG_FOOT",       "JOINT_LEFT_UPLEG_LEG",      "JOINT_HIPS_LEFT_LEG",
        "JOINT_RIGHT_FOOT_TOE_BASE", "JOINT_RIGHT_LEG_FOOT",      "JOINT_RIGHT_UPLEG_LEG",
        "JOINT_HIPS_RIGHT_LEG",
    };
    S3G_TRY(const float swing, data.float_("physics_skeleton_joints", "default", "SwingRagdollScalar"));
    S3G_TRY(const float twist, data.float_("physics_skeleton_joints", "default", "TwistRagdollScalar"));
    Settings out{};
    for (std::size_t i = 0; i < names.size(); ++i) {
        S3G_TRY(const auto raw, data.words<5>("physics_skeleton_joints", "default", names[i]));
        const auto words = rs::from_bits(raw);
        const auto& bone = skeleton.bones[i + 1];
        const float s = rs::from_bits(bone.words[21]);
        const float t = rs::from_bits(bone.words[20]);
        out.normal_limits[i] = limits(s * words[1], t * words[2]);
        out.ragdoll_limits[i] = limits((s * words[3]) * swing, (t * words[4]) * twist);
    }
    auto material = [&](const char* friction_name, const char* restitution_name) -> rs::Result<RetailContactMaterial, std::string> {
        S3G_TRY(const float friction, data.float_("physics_skeleton", "default", friction_name));
        RetailContactMaterial m{};
        m.static_friction = friction;
        m.dynamic_friction = friction;
        S3G_TRY(m.restitution, data.float_("physics_skeleton", "default", restitution_name));
        return m;
    };
    const float scale = rs::from_bits(0x426fffffu);
    S3G_TRY(out.inverse_mass, data.boolean("physics_wipeout", "default", "DoInverseMass"));
    S3G_TRY(out.inverse_inertia, data.boolean("physics_wipeout", "default", "DoInverseInertia"));
    S3G_TRY(const float linear, data.float_("physics_wipeout", "default", "LinearDrag"));
    out.drag[0] = linear * scale;
    S3G_TRY(const float angular, data.float_("physics_wipeout", "default", "AngularDrag"));
    out.drag[1] = angular * scale;
    S3G_TRY(out.materials[0], material("FrictionRagdoll", "RestitutionRagdoll"));
    S3G_TRY(out.materials[1], material("FrictionRagdollHead", "RestitutionRagdollHead"));
    return out;
}

std::array<std::uint32_t, 4> limits(float swing_, float twist_) {
    const float swing = swing_ - 0.01f >= 0.0f ? swing_ : 0.01f;
    const float twist = twist_ - 0.01f >= 0.0f ? twist_ : 0.01f;
    return {rs::to_bits(swing), rs::to_bits(twist), rs::to_bits(::s3::trigonometry::cos(swing)),
            rs::to_bits(::s3::trigonometry::cos(twist))};
}

}  // namespace s3::game::physics::wipeout_states::ragdoll::settings
