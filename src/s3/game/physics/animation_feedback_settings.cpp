// Port of skate-game/src/physics/animation_feedback_settings.rs.
#include "s3/game/physics/animation_feedback_settings.hpp"

#include <algorithm>

namespace s3::game::physics::animation_feedback_settings {

using sc::point_graph::PointGraph;

rs::Result<sc::input::turn_conditioner::Settings, std::string> load(const sd::Collections& data) {
    auto coefficients = [&](const char* name) -> rs::Result<std::array<float, 4>, std::string> {
        S3G_TRY(auto w, data.words<4>("physics_animation", "default", name));
        return rs::from_bits(w);
    };
    auto plain = [&](const char* name) -> rs::Result<PointGraph<8>, std::string> {
        S3G_TRY(auto words, data.words<16>("physics_animation", "default", name));
        const auto w = rs::from_bits(words);
        PointGraph<8> g;
        std::copy(w.begin(), w.begin() + 8, g.x.begin());
        std::copy(w.begin() + 8, w.end(), g.y.begin());
        return g;
    };
    S3G_TRY(auto speed_words, data.words<20>("physics_animation", "default", "ScaleQuicknessAtSpeed"));
    const auto speed = rs::from_bits(speed_words);
    S3G_TRY(auto smoothing_words, data.words<12>("physics_animation", "default", "SpeedWobbleTurnBlend"));
    const auto smoothing = rs::from_bits(smoothing_words);
    std::array<float, 13> parameters{};
    const char* names[13] = {
        "QuicknessMaxDelta",      "InputQuicknessMax",      "InputQuicknessBlendVal",
        "HoldingMinVal",          "HoldingMaxAcceleration", "HoldingIntegralBlend",
        "HoldingBlendUp",         "HoldingBlendDown",       "AnimationTurnWobbleScalar",
        "AnimationTurnPhysicsWeight", "AnimationTurnMaxLean", "AnimationTurnGraphX",
        "AnimationTurnDirMax",
    };
    for (std::size_t i = 0; i < 13; ++i) {
        S3G_TRY(parameters[i], data.float_("physics_animation", "default", names[i]));
    }
    sc::input::turn_conditioner::Settings settings;
    S3G_TRY(settings.filter_coefficients[0], coefficients("AnimationTurnLeanFilterSlow"));
    S3G_TRY(settings.filter_coefficients[1], coefficients("AnimationTurnLeanFilter"));
    S3G_TRY(settings.filter_coefficients[2], coefficients("AnimationTurnDirFilter"));
    S3G_TRY(settings.input_curve, plain("AnimationTurnSpeedGraph"));
    S3G_TRY(settings.quickness_curve, plain("QuicknessVsQuickness"));
    std::copy(speed.begin() + 4, speed.begin() + 12, settings.speed_curve.x.begin());
    std::copy(speed.begin() + 12, speed.begin() + 20, settings.speed_curve.y.begin());
    std::copy(smoothing.begin() + 4, smoothing.begin() + 8, settings.smoothing_curve.x.begin());
    std::copy(smoothing.begin() + 8, smoothing.begin() + 12, settings.smoothing_curve.y.begin());
    settings.parameters = parameters;
    return settings;
}

}  // namespace s3::game::physics::animation_feedback_settings
