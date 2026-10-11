// Port of skate-game/src/graph_host/motion_grind/settings.rs (module `motion_grind::settings`).
#pragma once
#include <array>
#include <string>

#include "s3/core/animation/grind_control.hpp"
#include "s3/core/rs.hpp"
#include "s3/data/collections.hpp"

namespace s3::game::graph_host::motion_grind::settings {

struct Settings {
    s3::animation::grind_control::FadeSettings fade;
    std::array<float, 3> height{};

    static rs::Result<Settings, std::string> load(const s3::data::Collections& d);
};

}  // namespace s3::game::graph_host::motion_grind::settings
