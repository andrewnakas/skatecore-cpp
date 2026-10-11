// Port of skate-game/src/physics/known_air/settings.rs.
//! Original KnownAir82D35508/82D35F40/82D360B0/82D36650 stock reads.
#include "s3/game/physics/known_air/settings.hpp"

#include <vector>

namespace s3::game::physics::known_air::settings {

using ::s3::air::known::KnownAirModeSettings;
using ::s3::air::known::KnownAirSettings;
using ::s3::point_graph::PointGraph;

rs::Result<std::pair<KnownAirSettings, std::array<KnownAirModeSettings, 5>>, std::string> load(
    const sd::Collections& data) {
    auto scalar = [&](const char* name) { return data.float_("physics_airstates", "default", name); };
    auto graph = [&](const char* name) -> rs::Result<PointGraph<8>, std::string> {
        S3G_TRY(const auto words, data.words<20>("physics_airstates", "default", name));
        const auto values = rs::from_bits(words);
        PointGraph<8> g;
        for (std::size_t i = 0; i < 8; ++i) {
            g.x[i] = values[4 + i];
            g.y[i] = values[12 + i];
        }
        return g;
    };
    KnownAirSettings settings;
    S3G_TRY(settings.max_heading_adjust_vs_up_y_160, graph("MaxHeadingAdjustVsUpY"));
    S3G_TRY(settings.landing_speed_scalar_vs_ground_normal_y_240, graph("LandingSpeedScalarVsGroundNormalY"));
    S3G_TRY(settings.flip_start_collision_time_vs_normal_y, graph("Hash_AA79C93673533C5B"));
    S3G_TRY(settings.trajectory_error_blend_away_time_384, scalar("TrajErrorBlendAwayTime"));
    S3G_TRY(settings.min_target_heading_velocity_420, scalar("MinTargetHeadingVel"));
    S3G_TRY(settings.min_auto_body_speed_424, scalar("MinAutoBodySpeed"));
    S3G_TRY(settings.max_spin_speed_428, scalar("MaxSpinSpeed"));
    S3G_TRY(settings.frames_for_grind_air_assist_436, scalar("FramesForGrindAirAssist"));
    S3G_TRY(settings.body_flip_min_grab_time_fraction_456, scalar("BodyFlipMinGrabTimeFraction"));
    S3G_TRY(settings.flip_scalar, data.float_("physics_reckoning", "default", "FlipScalar"));
    std::vector<KnownAirModeSettings> modes;
    modes.reserve(5);
    for (const char* key : {"easy", "normal", "hardcore", "motorized", "test"}) {
        KnownAirModeSettings m;
        S3G_TRY(m.upside_down_falling_wipeout_enabled_2, data.boolean("physics_mode", key, "Hash_2B913C786BEFF4CD"));
        S3G_TRY(m.perfect_body_flips_28, data.boolean("physics_mode", key, "PerfectBodyFlips"));
        S3G_TRY(m.body_spin_speed_limit_56, data.float_("physics_mode", key, "MaxAutoBodySpinSpeed"));
        modes.push_back(m);
    }
    if (modes.size() != 5) return rs::make_err(std::string("Expected five physics modes"));
    std::array<KnownAirModeSettings, 5> out{};
    for (std::size_t i = 0; i < 5; ++i) out[i] = modes[i];
    return std::make_pair(settings, out);
}

}  // namespace s3::game::physics::known_air::settings
