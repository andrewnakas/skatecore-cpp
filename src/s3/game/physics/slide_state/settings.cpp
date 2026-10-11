// Port of skate-game/src/physics/slide_state/settings.rs.
//! Direct original bindings82D94EC8..4F48,82D3AB30/AD3C/AE68 and stock schema.
#include "s3/game/physics/slide_state/settings.hpp"

#include "s3/game/physics/slide_state.hpp"

namespace s3::game::physics::slide_state {

rs::Result<SlideState, std::string> SlideState::load(const sd::Collections& data) {
    auto f = [&](const char* class_, const char* field) { return data.float_(class_, "default", field); };
    auto c = [&](const char* field) { return settings::curve(data, "physics_slide", "default", field); };
    std::vector<std::pair<SlideSurface, RetailContactMaterial>> surfaces;
    for (const char* key : {"smooth", "rough", "slow", "slippery", "veryslow"}) {
        SlideSurface surface;
        S3G_TRY(surface.speed_to_force, settings::curve(data, "physics_surfaces", key, "Powerslide_SpeedToForce"));
        S3G_TRY(surface.yaw_strength, data.float_("physics_surfaces", key, "Powerslide_YawStrength"));
        S3G_TRY(surface.yaw_damping, data.float_("physics_surfaces", key, "Powerslide_YawDamping"));
        RetailContactMaterial material;
        S3G_TRY(material.static_friction, data.float_("physics_surfaces", key, "WheelStaticFriction"));
        S3G_TRY(material.dynamic_friction, data.float_("physics_surfaces", key, "WheelDynamicFriction"));
        S3G_TRY(material.restitution, f("physicswheels", "WheelRestitution"));
        surfaces.emplace_back(surface, material);
    }
    SlideState out;
    out.state = sc::player::slide_state::SlideState::new_();
    S3G_TRY(out.settings.input_remap, c("slide_input_remap"));
    S3G_TRY(out.settings.remap_vs_speed, c("Hash_DE162591FD9D91D7"));
    // Toolkit528 copies layout160;608 copies layout80, not table order.
    S3G_TRY(out.settings.force_vs_angle, c("Hash_91282E2CC4252731"));
    S3G_TRY(out.settings.force_vs_speed, c("Hash_2AD93E5ABA231D2"));
    S3G_TRY(out.settings.softest_wheel_force, f("physicswheels", "SoftestWheelPowerslideFactor"));
    S3G_TRY(out.settings.softest_wheel_spin, f("physicswheels", "SoftestWheelPowerslideSpinFactor"));
    S3G_TRY(out.settings.angular_force, f("physics_slide", "AngularForceScalar"));
    S3G_TRY(out.settings.force_y_offset, f("physics_slide", "PowerSlideForceYOffset"));
    out.surfaces = std::move(surfaces);
    S3G_TRY(out.manual_scalar, f("physics_manual", "PowerSlideScalar"));
    return out;
}

}  // namespace s3::game::physics::slide_state

namespace s3::game::physics::slide_state::settings {

rs::Result<sc::point_graph::PointGraph<8>, std::string> curve(const sd::Collections& d, std::string_view c,
                                                              std::string_view k, std::string_view n) {
    S3G_TRY(const auto words, d.words<20>(std::string(c), std::string(k), std::string(n)));
    const auto w = rs::from_bits(words);
    sc::point_graph::PointGraph<8> g;
    for (std::size_t i = 0; i < 8; ++i) {
        g.x[i] = w[4 + i];
        g.y[i] = w[12 + i];
    }
    return g;
}

}  // namespace s3::game::physics::slide_state::settings
