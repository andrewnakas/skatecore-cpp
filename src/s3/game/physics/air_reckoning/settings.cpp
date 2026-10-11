// Port of skate-game/src/physics/air_reckoning/settings.rs.
//! Stock classes are verified against original8289D180 and828A0054..00A0:
//! physics_reckoning7CE1B6B581709C6F, physics_bodyspin05EC9CB03A5B2EEE.
#include "s3/game/physics/air_reckoning/settings.hpp"

#include <vector>

namespace s3::game::physics::air_reckoning::settings {

using ::s3::air::reckoning::Settings;
using ::s3::point_graph::PointGraph;

rs::Result<std::pair<Settings, std::array<Mode, 5>>, std::string> load(const sd::Collections& data) {
    auto float_ = [&](const char* class_, const char* field) { return data.float_(class_, "default", field); };
    S3G_TRY(const auto control_words, data.words<4>("physics_reckoning", "default", "GroundNormalSmoothing"));
    const auto control = rs::from_bits(control_words);
    std::vector<Mode> modes;
    modes.reserve(5);
    //Player ctor82DB1B08..1B78 installs these exact physics_mode keys.
    for (const char* name : {"easy", "normal", "hardcore", "motorized", "test"}) {
        Mode m;
        S3G_TRY(m.easy_body_spins, data.boolean("physics_mode", name, "EasyBodySpins"));
        S3G_TRY(m.perfect_body_flips, data.boolean("physics_mode", name, "PerfectBodyFlips"));
        modes.push_back(m);
    }
    Settings s;
    s.ground_normal_smoothing = control;
    S3G_TRY(s.max_up_angle_delta, negative_graph(data, "physics_reckoning", "Air_MaxUpVectAngleDelta"));
    S3G_TRY(s.tilt_vs_rotation, graph(data, "TiltVsRotAir"));
    S3G_TRY(s.tilt_vs_slope, graph(data, "TiltVsSlopeAir"));
    S3G_TRY(s.body_spin.derivative_floor, float_("physics_bodyspin", "MinDerivativeScalar"));
    S3G_TRY(s.body_spin.acceleration_limit, float_("physics_bodyspin", "MaxDeltaOppositeDirection"));
    //Original82D8BE38 copies layouts160,240,320,480,400,80,0.
    S3G_TRY(s.body_spin.curves[0], negative_graph(data, "physics_bodyspin", "Hash_BEA30B5DC6AFF26A"));
    S3G_TRY(s.body_spin.curves[1], negative_graph(data, "physics_bodyspin", "MaxDeltaVsTime"));
    S3G_TRY(s.body_spin.curves[2], negative_graph(data, "physics_bodyspin", "MaxDeltaForAutoVsTime"));
    S3G_TRY(s.body_spin.curves[3], negative_graph(data, "physics_bodyspin", "Hash_3974F0228331EB22"));
    S3G_TRY(s.body_spin.curves[4], negative_graph(data, "physics_bodyspin", "DerivativeBodySpinVsTime"));
    S3G_TRY(s.body_spin.curves[5], negative_graph(data, "physics_bodyspin", "Hash_D7C6855B7814D048"));
    S3G_TRY(s.body_spin.curves[6], negative_graph(data, "physics_bodyspin", "PropBodySpinVsTime"));
    //82F82690 splats original821647E0 into830BD300.
    const float fade = rs::from_bits(0x37800000u);
    s.body_spin.input_fade_threshold = {fade, fade, fade, fade};
    S3G_TRY(const float smoothing, float_("physics_reckoning", "FlipSpeedSmoothingFactor"));
    s.body_flip.smoothing = smoothing;
    S3G_TRY(const float maximum_speed, float_("physics_reckoning", "FlipMaxSpeed"));
    s.body_flip.maximum_speed = maximum_speed;
    S3G_TRY(const float spin_scale, float_("physics_reckoning", "FlipBodySpinScalar"));
    s.body_flip.spin_scale = spin_scale;
    //All three attributes are mandatory above; no missing-attribute
    //branch is reachable through this production settings loader.
    s.body_flip.missing_attribute_value = 0.0f;
    if (modes.size() != 5) return rs::make_err(std::string("Expected five physics modes"));
    std::array<Mode, 5> out{};
    for (std::size_t i = 0; i < 5; ++i) out[i] = modes[i];
    return std::make_pair(std::move(s), out);
}

rs::Result<PointGraph<8>, std::string> negative_graph(const sd::Collections& data, const std::string& class_,
                                                     const std::string& name) {
    S3G_TRY(const auto words, data.words<20>(class_, "default", name));
    const auto values = rs::from_bits(words);
    //Source directly calls PointGraphEval on +16/+48, ignoring sign metadata.
    PointGraph<8> g;
    for (std::size_t i = 0; i < 8; ++i) {
        g.x[i] = values[4 + i];
        g.y[i] = values[12 + i];
    }
    return g;
}

rs::Result<PointGraph<8>, std::string> graph(const sd::Collections& data, const std::string& name) {
    S3G_TRY(const auto words, data.words<16>("physics_reckoning", "default", name));
    const auto values = rs::from_bits(words);
    PointGraph<8> g;
    for (std::size_t i = 0; i < 8; ++i) {
        g.x[i] = values[i];
        g.y[i] = values[8 + i];
    }
    return g;
}

}  // namespace s3::game::physics::air_reckoning::settings
