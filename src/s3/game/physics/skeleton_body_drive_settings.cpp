// Port of skate-game/src/physics/skeleton_body_drive_settings.rs.
#include "s3/game/physics/skeleton_body_drive_settings.hpp"

namespace s3::game::physics::skeleton_body::drive_settings {

using sc::physics::skeleton_body::AnimationDriveSettings;

rs::Result<sc::physics::skeleton_body::BoneDriveSettings, std::string> load(const sd::Collections& data) {
    auto scalar = [&](const char* name) { return data.float_("physics_skeleton_drives", "default", name); };
    auto animation = [&](const char* key) -> rs::Result<AnimationDriveSettings, std::string> {
        auto f = [&](const char* name) { return data.float_("animation_drives", key, name); };
        AnimationDriveSettings s;
        S3G_TRY(s.linear_strength, f("LinearStrMin"));
        S3G_TRY(s.linear_displacement, f("LinearDispMin"));
        S3G_TRY(s.angular_strength, f("AngularStrMin"));
        S3G_TRY(s.angular_displacement, f("AngularDispMin"));
        return s;
    };
    sc::physics::skeleton_body::BoneDriveSettings out;
    S3G_TRY(out.animation[0], animation("local"));
    S3G_TRY(out.animation[1], animation("root"));
    S3G_TRY(out.collision_soft_displacement, scalar("CollisionSoftDisp"));
    S3G_TRY(out.collision_soft_strength, scalar("CollisionSoftStrength"));
    S3G_TRY(out.ragdoll_soft_displacement, scalar("RagdollSoftDisp"));
    S3G_TRY(out.ragdoll_soft_strength, scalar("RagdollSoftStrength"));
    S3G_TRY(out.transition_linear.spring[0], scalar("Hash_138976EAF99DF926"));
    S3G_TRY(out.transition_linear.spring[1], scalar("Hash_C313093118C7A83F"));
    S3G_TRY(out.transition_linear.strength[0], scalar("Hash_1E6BDF62043784FC"));
    S3G_TRY(out.transition_linear.strength[1], scalar("Hash_BD3E4F132848A588"));
    S3G_TRY(out.transition_linear.damping[0], scalar("Hash_D7652B4B19BB4747"));
    S3G_TRY(out.transition_linear.damping[1], scalar("Hash_C4B564DFD233CF04"));
    S3G_TRY(out.transition_angular.spring[0], scalar("Hash_14A2DDDD0A35C267"));
    S3G_TRY(out.transition_angular.spring[1], scalar("Hash_24D5AA7F214A901C"));
    S3G_TRY(out.transition_angular.strength[0], scalar("Hash_6C6CCD16A0A438AD"));
    S3G_TRY(out.transition_angular.strength[1], scalar("Hash_7379F8AB250BF611"));
    S3G_TRY(out.transition_angular.damping[0], scalar("Hash_28446A9E25DCC04C"));
    S3G_TRY(out.transition_angular.damping[1], scalar("Hash_B59D493B3DB372C8"));
    S3G_TRY(out.transition_calls, scalar("Hash_E1B89BB09D725EEC"));
    return out;
}

}  // namespace s3::game::physics::skeleton_body::drive_settings
