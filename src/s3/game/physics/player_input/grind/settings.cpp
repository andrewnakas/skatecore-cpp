// Port of skate-game/src/physics/player_input/grind/settings.rs.
#include "s3/game/physics/player_input/grind/settings.hpp"

namespace s3::game::physics::player_input::grind::settings {

rs::Result<Settings, std::string> Settings::load(const sd::Collections& data) {
    auto f = [&](const char* name) { return data.float_("physics_grinds", "default", name); };
    S3G_TRY(const auto exit_words, data.words<20>("physics_grinds", "default", "ExitLeanAngleVsTime"));
    const auto exit = rs::from_bits(exit_words);
    Settings s;
    S3G_TRY(s.friction, graph4(data, "FrictionVsTime"));
    S3G_TRY(s.slope_threshold, graph4(data, "DVEntryThreshScalarVsSinSlope"));
    S3G_TRY(s.vertical_help, graph4(data, "VertEngagementHelpVsVelY"));
    S3G_TRY(s.gravity_vertical, graph4(data, "TimeOfGravityReliefVsVerticalSpeed"));
    S3G_TRY(s.gravity_linear, graph4(data, "GravityReliefTimeVsLinearSpeed"));
    for (std::size_t i = 0; i < 8; ++i) {
        s.exit_lean.x[i] = exit[4 + i];
        s.exit_lean.y[i] = exit[12 + i];
    }
    S3G_TRY(s.truck_to_wheel, f("TruckToWheel"));
    S3G_TRY(s.deck_to_truck, f("DeckCenterToTruck"));
    S3G_TRY(s.test_above, f("TestDepthEpsilon"));
    S3G_TRY(s.test_below, f("TestDepth"));
    // physics_wipeout layout256,82D87358; not trajectory grind speed.
    S3G_TRY(s.max_impact, data.float_("physics_wipeout", "default", "Wipeout_AirMaxSpeedIntoCollisionNearGrind"));
    return s;
}

rs::Result<PointGraph<4>, std::string> graph4(const sd::Collections& data, std::string_view name) {
    S3G_TRY(const auto raw_words, data.words<12>("physics_grinds", "default", std::string(name)));
    const auto words = rs::from_bits(raw_words);
    // PointNegGraphData4 has a16-byte display-domain header before X/Y.
    PointGraph<4> g;
    for (std::size_t i = 0; i < 4; ++i) {
        g.x[i] = words[4 + i];
        g.y[i] = words[8 + i];
    }
    return g;
}

}  // namespace s3::game::physics::player_input::grind::settings
