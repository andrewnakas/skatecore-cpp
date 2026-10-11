// Port of skate-game/src/physics/ground_animation/settings.rs.
//!82D94C10 copies physics_jump layout0..456 into the board toolkit cache.
#include "s3/game/physics/ground_animation/settings.hpp"

namespace s3::game::physics::ground_animation::settings {

using sc::air::ground_jump::GroundJumpMode;
using sc::point_graph::PointGraph;

rs::Result<GroundAnimationSettings, std::string> GroundAnimationSettings::load(const sd::Collections& d) {
    auto f = [&](const char* name) { return d.float_("physics_jump", "default", name); };
    S3G_TRY(const auto words, d.words<32>("physics_jump", "default", "VerticalResponse"));
    std::array<GroundJumpMode, 5> modes{};
    for (auto& m : modes) {
        m.minimum_height_64 = 0.0f;
        m.minimum_height_68 = 0.0f;
        m.maximum_height = 0.0f;
    }
    std::size_t slot = 0;
    for (const char* key : {"easy", "normal", "hardcore", "motorized", "test"}) {
        GroundJumpMode m;
        S3G_TRY(m.minimum_height_64, d.float_("physics_mode", key, "Hash_BE3F74F978D777E5"));
        S3G_TRY(m.minimum_height_68, d.float_("physics_mode", key, "JumpMinHeight"));
        S3G_TRY(m.maximum_height, d.float_("physics_mode", key, "JumpMaxHeight"));
        modes[slot++] = m;
    }
    GroundAnimationSettings out;
    out.modes = modes;
    auto& j = out.jump;
    for (std::size_t i = 0; i < 16; ++i) {
        j.vertical_response.x[i] = rs::from_bits(words[i]);
        j.vertical_response.y[i] = rs::from_bits(words[16 + i]);
    }
    S3G_TRY(j.y_scalar_vs_normal_y, graph(d, "JumpYScalarVsGroundNormalY", true));
    S3G_TRY(j.speed_scalar_vs_angle, graph(d, "JumpSpeedScalarVsAngle", true));
    S3G_TRY(j.minimum_height_vs_speed, graph(d, "MinHeightVsSpeed", false));
    S3G_TRY(j.maximum_height_vs_speed, graph(d, "MaxHeightVsSpeed", false));
    S3G_TRY(j.speed_response_max_speed, f("SpeedResponseMaxSpeed"));
    S3G_TRY(j.minimum_scalar, f("MinScalar"));
    S3G_TRY(j.maximum_y_bonus, f("JumpYBonusMax"));
    S3G_TRY(j.adjust_z_factor, f("JumpAdjustZFactor"));
    S3G_TRY(j.adjust_x_factor, f("JumpAdjustXFactor"));
    S3G_TRY(j.absolute_minimum_height, f("AbsoluteMinHeight"));
    S3G_TRY(j.hippy_minimum_height, f("HippyJumpMinHeight"));
    S3G_TRY(j.hippy_maximum_height, f("HippyJumpMaxHeight"));
    return out;
}

rs::Result<PointGraph<8>, std::string> graph(const sd::Collections& d, std::string_view name_, bool negative) {
    const std::string name(name_);
    PointGraph<8> g;
    if (negative) {
        S3G_TRY(const auto w, d.words<20>("physics_jump", "default", name));
        for (std::size_t i = 0; i < 8; ++i) {
            g.x[i] = rs::from_bits(w[4 + i]);
            g.y[i] = rs::from_bits(w[12 + i]);
        }
    } else {
        S3G_TRY(const auto w, d.words<16>("physics_jump", "default", name));
        for (std::size_t i = 0; i < 8; ++i) {
            g.x[i] = rs::from_bits(w[i]);
            g.y[i] = rs::from_bits(w[8 + i]);
        }
    }
    return g;
}

}  // namespace s3::game::physics::ground_animation::settings
