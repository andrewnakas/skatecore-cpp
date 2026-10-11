// Port of skate-game/src/physics/offboard/board_possession/settings.rs
#include "s3/game/physics/offboard/board_possession/settings.hpp"

namespace s3::game::physics::offboard::board_possession::settings {

rs::Result<float, std::string> standard_angular_drag(const sd::Collections& data) {
    S3G_TRY(float drag, data.float_("physicsdeck", "default", "DeckAngularDrag"));
    return drag * rs::from_bits(0x426fffffu);
}

rs::Result<Settings, std::string> load(const sd::Collections& data) {
    auto float_ = [&](const char* name) { return data.float_("physics_skatecontroller", "default", name); };
    Settings s{};
    S3G_TRY(s.hide_distance, float_("MaxDistance"));
    S3G_TRY(s.hide_offset, float_("DistanceToHideSkateboard"));
    S3G_TRY(s.return_distance, float_("DistanceForBoardReturn"));
    S3G_TRY(s.mounted_return_distance, float_("DistanceForMountedReturn"));
    S3G_TRY(s.mounting_time, float_("MinTimeToRetrieveBoardWhileMounting"));
    S3G_TRY(s.retrieval_time, curve(data, "RetrievalTimeVsDist"));
    S3G_TRY(s.retrieval_weight, curve(data, "RetrievalSpeedCurve"));
    S3G_TRY(s.throw_pitch, float_("ThrownSkateboardLaunchPitch"));
    S3G_TRY(s.throw_velocity, curve(data, "ThrownSkateboardVelocity"));
    S3G_TRY(s.throw_target_pitch, float_("ThrownSkateboardPitchTarget"));
    S3G_TRY(s.throw_pitch_scalar, float_("ThrownSkateboardPitchScalar"));
    S3G_TRY(s.throw_roll_scalar, float_("ThrownSkateboardRollScalar"));
    S3G_TRY(s.throw_yaw_scalar, float_("ThrownSkateboardYawScalar"));
    return s;
}

rs::Result<sc::point_graph::PointGraph<8>, std::string> curve(const sd::Collections& data, const std::string& name) {
    S3G_TRY(const sd::Field* f, data.field("physics_skatecontroller", "default", name));
    if (f->type_name != "Sk8::PointNegGraphData8") {
        return rs::make_err(fmt::cat(name, ": expected PointNegGraphData8"));
    }
    S3G_TRY(auto raw, data.words<20>("physics_skatecontroller", "default", name));
    const std::array<float, 20> words = rs::from_bits(raw);
    for (float v : words) {
        if (!rs::is_finite(v)) return rs::make_err(fmt::cat(name, ": nonfinite stock graph"));
    }
    sc::point_graph::PointGraph<8> g{};
    for (std::size_t i = 0; i < 8; ++i) g.x[i] = words[4 + i];
    for (std::size_t i = 0; i + 1 < 8; ++i) {
        if (g.x[i] > g.x[i + 1]) return rs::make_err(fmt::cat(name, ": unordered stock knots"));
    }
    for (std::size_t i = 0; i < 8; ++i) g.y[i] = words[12 + i];
    return g;
}

}  // namespace s3::game::physics::offboard::board_possession::settings
