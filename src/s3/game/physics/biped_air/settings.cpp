// Port of skate-game/src/physics/biped_air/settings.rs
//! Stock physics_wipeout fields read by82D8FEE8/82D90000 only.
#include "s3/game/physics/biped_air/settings.hpp"

namespace s3::game::physics::biped_air::settings {

rs::Result<Settings, std::string> load(const sd::Collections& data) {
    auto f = [&](const char* name) { return data.float_("physics_wipeout", "default", name); };
    Settings s{};
    S3G_TRY(s.skeleton_air.squash, f("Wipeout_AirMaxSquash"));
    S3G_TRY(s.skeleton_air.displacement, f("Wipeout_AirSkeletonMaxDisp"));
    S3G_TRY(s.skeleton_air.body_contact, f("Wipeout_AirSkeletonMaxContact"));
    S3G_TRY(s.skeleton_air.arm_contact, f("Wipeout_AirSkeletonMaxContactArms"));
    S3G_TRY(s.offboard_air.squash, f("Wipeout_OB_MaxSquash"));
    S3G_TRY(s.offboard_air.displacement, f("Wipeout_OB_Air_SkelMaxDisp"));
    S3G_TRY(s.offboard_air.body_contact, f("Wipeout_OB_Air_SkelMaxContact"));
    S3G_TRY(s.offboard_air.arm_contact, f("Wipeout_OB_SkeletonMaxContactArms"));
    S3G_TRY(s.offboard_min_speed, f("Wipeout_OB_Air_MinSpeed"));
    return s;
}

}  // namespace s3::game::physics::biped_air::settings
