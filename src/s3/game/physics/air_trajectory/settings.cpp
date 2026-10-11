// Port of skate-game/src/physics/air_trajectory/settings.rs.
//! Stock class hashes429A8F4385DD539D(trajectory),7CE1B6B581709C6F
//! (reckoning),5338958C408B6009(feet), defaultD7EDBD362D7D2152.
//! Layout names/offsets were checked against original TU3 loads and stock XML.
#include "s3/game/physics/air_trajectory/settings.hpp"

namespace s3::game::physics::air_trajectory::settings {

using ::s3::air::trajectory::SelectorSettings;
using ::s3::point_graph::PointGraph;

rs::Result<SelectorSettings, std::string> load(const sd::Collections& data) {
    auto t = [&](const char* name) { return data.float_("physics_trajectory", "default", name); };
    auto r = [&](const char* name) { return data.float_("physics_reckoning", "default", name); };
    SelectorSettings s;
    S3G_TRY(s.cone_x, t("ConeAngleX"));
    S3G_TRY(s.cone_z, t("ConeAngleZ"));
    S3G_TRY(s.trajectory_max_time, t("TrajectoryMaxTime"));
    S3G_TRY(s.trajectory_max_drop, t("TrajectoryMaxDrop"));  //63673FB6DA092B7E
    S3G_TRY(s.trajectory_error_start, t("TrajectoryMaxErrorStart"));
    S3G_TRY(s.trajectory_error_end, t("TrajectoryMaxErrorEnd"));
    S3G_TRY(s.speed_factor_min, t("SpeedFactorMin"));
    S3G_TRY(s.speed_factor_max, t("SpeedFactorMax"));
    S3G_TRY(s.cone_angle_z_vs_speed, graph8(data, "physics_trajectory", "ConeAngleZVsSpeed", false));
    S3G_TRY(s.cone_x_second_pass, t("ConeAngleXSecondPass"));
    S3G_TRY(s.cone_z_second_pass, t("ConeAngleZSecondPass"));
    S3G_TRY(s.landing_time_bonus, graph8(data, "physics_trajectory", "LandingTimeBonus", true));
    S3G_TRY(s.landing_com_scalar_vs_slope, graph8(data, "physics_trajectory", "LandingComScalarVsSlope", true));
    S3G_TRY(s.landing_force_scalar, graph8(data, "physics_trajectory", "LandingForceScalarVsDPVelNorm", false));
    S3G_TRY(s.grind_penalty_vs_distance, graph8(data, "physics_trajectory", "GrindPenaltyVsDistToGrind", true));
    S3G_TRY(s.score_middle_bonus, t("ScoreMiddleBonus"));
    S3G_TRY(s.score_landing_force, t("ScoreLandingForce"));
    S3G_TRY(s.score_landing_direction, t("ScoreLandingDirection"));
    S3G_TRY(s.score_transition, t("ScoreTransition"));
    S3G_TRY(s.surface_unrideable_score, t("ScoreSurface_Unrideable"));
    S3G_TRY(s.surface_dont_align_score, t("ScoreSurface_DontAlign"));
    S3G_TRY(s.natural_air_off_verts_scalar, t("NaturalAirOffVertsScalar"));
    S3G_TRY(s.minimum_valid_time, t("MinTimeForTrajectoryToBeValid"));
    S3G_TRY(s.minimum_time_after_apex, t("MinTimeAfterApexForTransition"));
    S3G_TRY(s.minimum_normal_delta_second_pass, t("MinNormalDeltaForSecondPass"));
    S3G_TRY(s.maximum_trajectory_adjust, t("MaxTrajectoryAdjust"));
    S3G_TRY(s.wall_ride_test_distance, t("WallRideTestDist"));
    S3G_TRY(s.wall_ride_minimum_height, t("WallRideMinHeightFromGround"));
    S3G_TRY(s.wall_ride_angle_allow_landing, t("WallRideAngleToAllowLanding"));
    S3G_TRY(s.wall_ride_height_score, t("ScoreWallrideHeight"));
    S3G_TRY(s.wall_ride_boost, negative_graph4(data, "physics_trajectory", "WallRideBoost"));
    //Globals8289D5C8:ED14 class lookup;ED48 stores188, layout248.
    S3G_TRY(s.wall_ride_normal_dot_limit, data.float_("physics_feet", "default", "WallRideMaxDotFloorWall"));
    S3G_TRY(s.displacement_vs_speed, graph8(data, "physics_reckoning", "TrajectoryDispVsSpeed", false));
    S3G_TRY(s.displacement_vs_ground_normal, graph8(data, "physics_reckoning", "TrajectoryDispVsGroundNorm", false));
    S3G_TRY(s.trajectory_radius, r("TrajectoryRadius"));              //C6710942E297D587
    S3G_TRY(s.trajectory_displacement, r("TrajectoryDisplacement"));  //F85A91F6A233DDAE
    S3G_TRY(const std::uint32_t min_size, data.integer("physics_reckoning", "default", "MinTrajectorySize"));
    s.minimum_trajectory_frames = static_cast<std::int32_t>(min_size);
    S3G_TRY(s.vert_jump_align_factor, r("VertJumpAlignFactor"));                         //0B42DFBB7F7756A0
    S3G_TRY(s.vert_jump_align_max_ground_normal_y, r("VertJumpAlignMaxGroundNormalY"));  //94923D136EA2DA37
    S3G_TRY(s.vert_jump_align_min_direction_y, r("VertJumpAlignMinJumpDirY"));           //58BFC8D41216C7EE
    S3G_TRY(s.vert_jump_align_max_angle, r("VertJumpAlignMaxAngle"));                    //723FF102068622CF
    return s;
}

rs::Result<PointGraph<8>, std::string> graph8(const sd::Collections& data, const std::string& class_,
                                             const std::string& name, bool negative) {
    const char* expected = negative ? "Sk8::PointNegGraphData8" : "Sk8::PointGraphData8";
    S3G_TRY(const sd::Field* field, data.field(class_, "default", name));
    if (field->type_name != expected) {
        return rs::make_err(fmt::cat("Wrong stock graph type ", class_, "/", name));
    }
    PointGraph<8> g;
    if (negative) {
        S3G_TRY(const auto words, data.words<20>(class_, "default", name));
        for (std::size_t i = 0; i < 8; ++i) {
            g.x[i] = rs::from_bits(words[4 + i]);
            g.y[i] = rs::from_bits(words[12 + i]);
        }
    } else {
        S3G_TRY(const auto words, data.words<16>(class_, "default", name));
        for (std::size_t i = 0; i < 8; ++i) {
            g.x[i] = rs::from_bits(words[i]);
            g.y[i] = rs::from_bits(words[8 + i]);
        }
    }
    //82481E10 receives the x/y spans after any PointNeg header.
    return g;
}

rs::Result<PointGraph<4>, std::string> negative_graph4(const sd::Collections& data, const std::string& class_,
                                                      const std::string& name) {
    S3G_TRY(const sd::Field* field, data.field(class_, "default", name));
    if (field->type_name != "Sk8::PointNegGraphData4") {
        return rs::make_err(fmt::cat("Wrong stock graph type ", class_, "/", name));
    }
    S3G_TRY(const auto words, data.words<12>(class_, "default", name));
    PointGraph<4> g;
    for (std::size_t i = 0; i < 4; ++i) {
        g.x[i] = rs::from_bits(words[4 + i]);
        g.y[i] = rs::from_bits(words[8 + i]);
    }
    return g;
}

}  // namespace s3::game::physics::air_trajectory::settings
