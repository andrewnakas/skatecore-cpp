// Port of skate-game/src/physics/landing_quality.rs.
//! Stock PhysicsAnimation cache +560/+576 and +592/+608, TU3 82DE6480.
#include "s3/game/physics/landing_quality.hpp"

#include "s3/core/point_graph.hpp"

namespace s3::game::physics::landing_quality {

rs::Result<sc::animation::landing_quality::Settings, std::string> load(const sd::Collections& data) {
    auto graph = [&](const char* name) -> rs::Result<sc::point_graph::PointGraph<4>, std::string> {
        S3G_TRY(const auto words, data.words<8>("physics_animation", "default", name));
        sc::point_graph::PointGraph<4> g{};
        for (std::size_t i = 0; i < 4; ++i) g.x[i] = rs::from_bits(words[i]);
        for (std::size_t i = 0; i < 4; ++i) g.y[i] = rs::from_bits(words[4 + i]);
        return g;
    };
    sc::animation::landing_quality::Settings out{};
    S3G_TRY(out.twist_spin, graph("LandingSketchyTwistSpin"));
    S3G_TRY(out.side_speed, graph("LandingSketchySideSpeed"));
    return out;
}

}  // namespace s3::game::physics::landing_quality
