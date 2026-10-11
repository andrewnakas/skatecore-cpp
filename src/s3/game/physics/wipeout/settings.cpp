// Port of skate-game/src/physics/wipeout/settings.rs
//! Globals264 -> original8289D270 class EB844CFDBF928EFD/default.
//! Fields are independently mapped through the stock physics_wipeout schema.
#include "s3/game/physics/wipeout/settings.hpp"

#include <vector>

namespace s3::game::physics::wipeout::settings {

using ::s3::player::wipeout::AirSettings;
using ::s3::player::wipeout::GroundSettings;
using ::s3::player::wipeout::Mode;
using ::s3::player::wipeout::Settings;

rs::Result<std::pair<Settings, std::array<Mode, 5>>, std::string> load(const sd::Collections& data) {
    auto f = [&](const char* name) { return data.float_("physics_wipeout", "default", name); };
    S3G_TRY(const auto graph_words, data.words<20>("physics_wipeout", "default", "Hash_4F08D9BAE6831524"));
    const auto graph = rs::from_bits(graph_words);
    std::vector<Mode> modes;
    for (const char* name : {"easy", "normal", "hardcore", "motorized", "test"}) {
        Mode m;
        S3G_TRY(m.check_squash, data.boolean("physics_mode", name, "Hash_CC890A3BDCF6290A"));
        S3G_TRY(m.check_bad_landing, data.boolean("physics_mode", name, "WipeoutCheckForBadLanding"));
        S3G_TRY(m.ground_xz, data.float_("physics_mode", name, "Wipeout_GroundXZAcceleration"));
        //82D90D40..94 constructs full64 D978550D6DB4E6F7.
        S3G_TRY(m.bad_landing_scale, data.float_("physics_mode", name, "Hash_D978550D6DB4E6F7"));
        modes.push_back(m);
    }
    Settings s;
    GroundSettings& g = s.ground;
    S3G_TRY(g.vehicle_scalar, f("Wipeout_GroundVehicleScalar"));
    S3G_TRY(g.vehicle_contact, f("Wipeout_GroundVehicleContact"));
    S3G_TRY(g.skitch_contact, f("Wipeout_GroundSkitchingContact"));
    S3G_TRY(g.skitch_scalar, f("Wipeout_GroundSkitchingScalar"));
    S3G_TRY(g.skitch_arms_scalar, f("Wipeout_GroundSkitchingScalarArms"));
    S3G_TRY(g.skater_scalar, f("SkaterSkaterThresholdScalar"));
    S3G_TRY(g.max_squash, f("Wipeout_GroundMaxSquash"));
    S3G_TRY(g.max_squash_coffin, f("Wipeout_GroundMaxSquashCoffin"));
    S3G_TRY(g.max_displacement, f("Wipeout_GroundSkeletonMaxDisp"));
    S3G_TRY(g.max_contact, f("Wipeout_GroundSkeletonMaxContact"));
    S3G_TRY(g.max_arm_contact, f("Wipeout_GroundSkeletonMaxContactArms"));
    S3G_TRY(g.max_deck_error, f("Wipeout_GroundMaxAngularDeckError"));
    S3G_TRY(g.opposing_contact, f("Wipeout_GroundOpposingContact"));
    S3G_TRY(g.y_acceleration, f("Wipeout_GroundYAcceleration"));
    S3G_TRY(g.light_dmo_scalar, f("WipeoutGroundLightDMOScalar"));
    S3G_TRY(g.player_scalar, f("DeckAccPlayerScalar"));
    S3G_TRY(g.ai_scalar, f("DeckAccAIScalar"));
    S3G_TRY(g.skitch_acc_scalar, f("DeckAccSkitchScalar"));
    S3G_TRY(g.balance_total, f("Wipeout_GroundBalanceTotal"));
    S3G_TRY(g.balance_min_speed, f("Wipeout_GroundBalanceMinSpeed"));
    S3G_TRY(g.balance_base, f("Wipeout_GroundBalanceBase"));
    AirSettings& a = s.air;
    S3G_TRY(a.xz_trick, f("Wipeout_AirXZTrick"));
    S3G_TRY(a.y_trick, f("Wipeout_AirYTrick"));
    S3G_TRY(a.xz_acceleration, f("Wipeout_AirXZAcceleration"));
    S3G_TRY(a.y_acceleration, f("Wipeout_AirYAcceleration"));
    S3G_TRY(a.max_squash, f("Wipeout_AirMaxSquash"));
    S3G_TRY(a.max_displacement, f("Wipeout_AirSkeletonMaxDisp"));
    S3G_TRY(a.max_contact, f("Wipeout_AirSkeletonMaxContact"));
    S3G_TRY(a.max_arm_contact, f("Wipeout_AirSkeletonMaxContactArms"));
    S3G_TRY(a.body_flip_scalar, f("Wipeout_AirBodyFlipScalar"));
    S3G_TRY(a.body_flip_acc_scalar, f("DeckAccBodyFlipScalar"));
    S3G_TRY(a.light_dmo_scalar, f("WipeoutAirLightDMOScalar"));
    S3G_TRY(const std::uint32_t ignore,
            data.integer("physics_wipeout", "default", "FramesAtStartOfAirToIgnoreDangerZone"));
    a.ignore_danger_frames = std::int32_t(ignore);
    S3G_TRY(a.max_landing_speed, f("Wipeout_AirMaxSpeedIntoGround"));
    S3G_TRY(a.max_stairs_speed, f("Wipeout_AirMaxSpeedIntoStairs"));
    S3G_TRY(a.max_grind_speed, f("Wipeout_AirMaxSpeedIntoCollisionNearGrind"));
    for (std::size_t i = 0; i < 8; ++i) {
        a.max_landing_angle.x[i] = graph[4 + i];
        a.max_landing_angle.y[i] = graph[12 + i];
    }
    S3G_TRY(s.lean_contact_y, f("Wipeout_GroundLeanContactYThresh"));
    if (modes.size() != 5) return rs::make_err(std::string("Expected five wipeout physics modes"));
    std::array<Mode, 5> out{};
    for (std::size_t i = 0; i < 5; ++i) out[i] = modes[i];
    return std::make_pair(s, out);
}

}  // namespace s3::game::physics::wipeout::settings
