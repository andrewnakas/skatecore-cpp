// Port of skate-game/src/physics/skeleton_body_collision_settings.rs.
#include "s3/game/physics/skeleton_body_collision_settings.hpp"

#include "s3/core/physics/contact.hpp"
#include "s3/data/physics_skeleton.hpp"

namespace s3::game::physics::skeleton_body::collision_settings {

rs::Result<sc::physics::skeleton_body::SkeletonCollisionSettings, std::string> load(const sd::Vfs& vfs,
                                                                                     const std::string& asset_root,
                                                                                     const sd::Collections& data,
                                                                                     std::string_view bank_sha) {
    S3G_TRY(auto skeleton,
            sd::PhysicsSkeleton::load(vfs, sd::join_path(asset_root, "private/stock/physics-skeletons.json"),
                                      std::string(bank_sha), "PHYS_TPOSE"));
    if (skeleton.bones.size() != 24)
        return rs::make_err(std::string("Skeleton collision requires24 bone records"));
    std::array<bool, 24> compliant{};
    std::array<float, 24> priority{};
    compliant[0] = true;  //82BE7484; part0 priority is cleared by collisionReset.
    for (std::size_t part = 1; part < 24; ++part) {
        S3G_TRY(auto words,
                data.words<9>("physics_skeleton_drives", "default", "PART_" + skeleton.bones[part].name));
        compliant[part] = (words[7] >> 24) != 0;
        priority[part] = rs::from_bits(words[8]);
    }
    // XML baseC8840; frictionC8904, restitutionC88EC, enabledC8910,
    // effectC8918 => native offsets196,172,208,216 respectively.
    S3G_TRY(const float friction, data.float_("physics_skeleton", "default", "FrictionNormal"));
    sc::physics::skeleton_body::SkeletonCollisionSettings out;
    S3G_TRY(out.enabled, data.boolean("physics_skeleton", "default", "EnableCollision"));
    out.normal_material.static_friction = friction;
    out.normal_material.dynamic_friction = friction;
    S3G_TRY(out.normal_material.restitution, data.float_("physics_skeleton", "default", "RestitutionNormal"));
    out.compliant = compliant;
    out.priority = priority;
    S3G_TRY(out.effect_time, data.float_("physics_skeleton", "default", "CollisionEffectTime"));
    return out;
}

rs::Result<sc::physics::skeleton_body::SkeletonFeedbackSettings, std::string> feedback(
    const sd::Collections& data, sc::physics::skeleton_body::SkeletonCollisionSettings body) {
    auto value = [&](const char* name) { return data.float_("physics_collision", "default", name); };
    auto vector = [&](const char* name) -> rs::Result<std::array<float, 4>, std::string> {
        S3G_TRY(auto w, data.words<4>("physics_collision", "default", name));
        return rs::from_bits(w);
    };
    sc::physics::skeleton_body::SkeletonFeedbackSettings out;
    out.body = body;
    S3G_TRY(out.small_object_mass, data.float_("physics_skeleton", "default", "SmallObjectMassThreshold"));
    S3G_TRY(out.ground_plane_max_distance, value("GroundPlaneMaxDist"));  //136
    S3G_TRY(out.ground_plane_max_angle, value("GroundPlaneMaxAngle"));    //140
    S3G_TRY(out.skater_scalar, value("SkaterSkeletonScalar"));            //116
    S3G_TRY(out.ai_scalar, value("AISkeletonScalar"));                    //188
    S3G_TRY(out.groin_offset, vector("GroinLocalOffset"));                //0
    S3G_TRY(out.face_offset, vector("FaceLocalOffset"));                  //16
    S3G_TRY(out.groin_radius, value("GroinRadius"));                      //144
    S3G_TRY(out.face_radius, value("FaceRadius"));                        //168
    return out;
}

}  // namespace s3::game::physics::skeleton_body::collision_settings
