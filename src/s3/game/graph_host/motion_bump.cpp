// Port of skate-game/src/graph_host/motion_bump.rs (module `motion_bump`).
#include "s3/game/graph_host/motion_bump.hpp"

#include "s3/game/graph_host/motion_collections.hpp"

namespace s3::game::graph_host::motion_bump {

rs::Result<Settings, std::string> Settings::load(const s3::data::Collections& data) {
    auto value = [&](const char* name) { return motion_collections::float_(data, "anim_motion", "bumps", name); };
    Settings s;
    S3_MOTION_TRY(s.scale_x_acc, value("scale_x_acc"));
    S3_MOTION_TRY(s.min_bump_mag, value("min_bump_mag"));
    S3_MOTION_TRY(s.max_bump_mag, value("max_bump_mag"));
    S3_MOTION_TRY(s.min_bump_blend_value, value("min_bump_blend_value"));
    return s;
}

}  // namespace s3::game::graph_host::motion_bump
