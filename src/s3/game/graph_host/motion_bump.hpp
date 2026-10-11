// Port of skate-game/src/graph_host/motion_bump.rs (module `motion_bump`).
//! SetBumpCoefficients Begin82BB0660, original Skate 3 TU3.
#pragma once
#include <array>
#include <string>

#include "s3/core/animation/bump.hpp"
#include "s3/core/rs.hpp"
#include "s3/data/collections.hpp"

namespace s3::game::graph_host::motion_bump {

struct Settings {
    float scale_x_acc = 0.0f;
    float min_bump_mag = 0.0f;
    float max_bump_mag = 0.0f;
    float min_bump_blend_value = 0.0f;

    static rs::Result<Settings, std::string> load(const s3::data::Collections& data);

    /// Begin82BB0660 uses the retained Xenon arithmetic implementation.
    std::array<float, 2> coefficients(std::array<float, 4> acceleration, bool mirrored) const {
        s3::animation::bump::Settings settings;
        settings.scale_x_acc = scale_x_acc;
        settings.min_bump_mag = min_bump_mag;
        settings.max_bump_mag = max_bump_mag;
        settings.min_bump_blend_value = min_bump_blend_value;
        return s3::animation::bump::coefficients(acceleration, mirrored, settings);
    }
};

}  // namespace s3::game::graph_host::motion_bump
