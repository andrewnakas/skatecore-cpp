// Port of skate-game/src/physics/grind/substate/settings.rs.
#include "s3/game/physics/grind/substate/settings.hpp"

namespace s3::game::physics::grind::substate::settings {

rs::Result<Settings, std::string> Settings::load(const sd::Collections& data) {
    auto f = [&](const char* name) { return data.float_("physics_collision", "default", name); };
    S3G_TRY(const auto graph_raw, data.words<20>("physics_collision", "default", "CollisionTorqueVsAngle"));
    const auto graph = rs::from_bits(graph_raw);
    std::array<std::array<std::array<float, 2>, 3>, 5> vertical{};
    const char* modes[5] = {"easy", "normal", "hardcore", "motorized", "test"};
    const char* families[3][2] = {
        {"Hash_3097A69281990652", "Hash_FA4CDBAE0DFD1FAD"},
        {"Hash_B2B1170AFFC8AC69", "Hash_1B3E9F9C836D287D"},
        {"Hash_703829BD711E54DE", "Hash_F2473E9125079F0"},
    };
    for (std::size_t index = 0; index < 5; ++index) {
        for (std::size_t family = 0; family < 3; ++family) {
            S3G_TRY(const float a, data.float_("physics_mode", modes[index], families[family][0]));
            S3G_TRY(const float b, data.float_("physics_mode", modes[index], families[family][1]));
            vertical[index][family] = {a, b};
        }
    }
    Settings s;
    S3G_TRY(s.animated_board_threshold,
            data.float_("physics_animation", "default", "MaxDeckZAxisYForAnimatedDeck"));
    S3G_TRY(s.collision.maximum_velocity_delta, f("MaxVelDelta"));
    S3G_TRY(s.collision.force_y_offset, f("ForceYOffset"));
    S3G_TRY(s.collision.force_scalar, f("CollisionForceScalar"));
    S3G_TRY(s.collision.target_displacement_velocity, f("TargetDisplacementVel"));
    for (std::size_t i = 0; i < 8; ++i) {
        s.collision.torque_vs_angle.x[i] = graph[4 + i];
        s.collision.torque_vs_angle.y[i] = graph[12 + i];
    }
    s.vertical_ = vertical;
    return s;
}

rs::Result<float, std::string> Settings::vertical(Family family, std::uint32_t mode, float strength) const {
    if (static_cast<std::size_t>(mode) >= vertical_.size()) {
        return rs::make_err(fmt::cat("Invalid grind jump physics mode ", mode));
    }
    const auto& modes = vertical_[mode];
    //Virtual64:82D40D60 common (including Darkslide),82D41CE8 board,
    //82D42720 tip. No strength clamp exists in these scalar getters.
    std::size_t group = 0;
    if (family == Family::Boardslide) group = 1;
    else if (family == Family::Tipslide) group = 2;
    const float low = modes[group][0];
    const float high = modes[group][1];
    return rs::mul_add(1.0f - strength, low, high * strength);
}

float side(Family family, std::uint32_t geometry_kind) {
    switch (family) {
        //82D41320 reads processed1464, returning literal820997B8 or82165A10.
        case Family::Boardslide:
        case Family::Darkslide:
            return geometry_kind == 2 ? rs::from_bits(0x3f666666u) : 0.0f;
        //82D42AE8 literal820997B8.
        case Family::Backslash:
            return rs::from_bits(0x3f666666u);
        //82D41FB0 literal8209975C; shared50-50/tip/5-O virtual60.
        default:
            return rs::from_bits(0x3f000000u);
    }
}

}  // namespace s3::game::physics::grind::substate::settings
