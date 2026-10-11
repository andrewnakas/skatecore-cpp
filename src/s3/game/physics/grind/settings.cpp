// Port of skate-game/src/physics/grind/settings.rs.
#include "s3/game/physics/grind/settings.hpp"

namespace s3::game::physics::grind::settings {

namespace {
template <std::size_t N, std::size_t M>
std::array<float, N> slice(const std::array<float, M>& a, std::size_t from) {
    std::array<float, N> out{};
    for (std::size_t i = 0; i < N; ++i) out[i] = a.at(from + i);
    return out;
}
}  // namespace

rs::Result<Settings, std::string> Settings::load(const sd::Collections& data) {
    S3G_TRY(const auto words_raw, data.words<8>("physics_grinds", "default", "PinVsSlope"));
    const auto words = rs::from_bits(words_raw);
    S3G_TRY(const auto exit_raw, data.words<12>("physics_grinds", "default", "ExitAssistVsLeanAngle"));
    const auto exit = rs::from_bits(exit_raw);
    auto wipeout = [&](const char* name) { return data.float_("physics_wipeout", "default", name); };
    auto graph = [&](const char* name) -> rs::Result<::s3::point_graph::PointGraph<8>, std::string> {
        S3G_TRY(const auto raw, data.words<16>("physics_reckoning", "default", name));
        const auto w = rs::from_bits(raw);
        return ::s3::point_graph::PointGraph<8>{slice<8>(w, 0), slice<8>(w, 8)};
    };
    Settings s;
    S3G_TRY(const float deck_drag, data.float_("physicsdeck", "default", "DeckAngularDrag"));
    s.standard_angular_drag = deck_drag * rs::from_bits(0x426fffffu);
    s.pin_vs_slope = ::s3::point_graph::PointGraph<4>{slice<4>(words, 0), slice<4>(words, 4)};
    S3G_TRY(s.look_ahead, data.float_("physics_grinds", "default", "GrindLookAheadScalar"));
    s.exit_assist = ::s3::point_graph::PointGraph<4>{slice<4>(exit, 4), slice<4>(exit, 8)};
    // Original82D90898 loads164/168. Stock schema proves Ground
    // contact thresholds, not similarly named Air fields240/244.
    S3G_TRY(s.post.max_arm_contact_164, wipeout("Wipeout_GroundSkeletonMaxContactArms"));
    S3G_TRY(s.post.max_body_contact_168, wipeout("Wipeout_GroundSkeletonMaxContact"));
    S3G_TRY(s.post.xz_acceleration_204, wipeout("Wipeout_GrindXZDeck"));
    S3G_TRY(s.post.max_displacement_208, wipeout("Wipeout_GrindSkeletonMaxDisp"));
    S3G_TRY(s.post.max_angular_deck_error_212, wipeout("Wipeout_GrindMaxAngularDeckError"));
    S3G_TRY(const auto smoothing, data.words<4>("physics_reckoning", "default", "GroundNormalSmoothing"));
    s.reckoning.ground_normal_smoothing = rs::from_bits(smoothing);
    S3G_TRY(s.reckoning.tilt_vs_rotation, graph("TiltVsRotGround"));
    S3G_TRY(s.reckoning.tilt_vs_slope, graph("TiltVsSlopeGround"));
    S3G_TRY(s.substate, substate::settings::Settings::load(data));
    return s;
}

}  // namespace s3::game::physics::grind::settings
