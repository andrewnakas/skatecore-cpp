// Port of skate-game/src/graph_host/motion_hippy_jump.rs (module `motion_hippy_jump`).
#include "s3/game/graph_host/motion_hippy_jump.hpp"

#include <cassert>

#include "s3/core/animation/skeleton_input/name.hpp"
#include "s3/game/graph_host/motion_animation.hpp"
#include "s3/game/graph_host/motion_collections.hpp"

namespace s3::game::graph_host::motion_hippy_jump {

using s3::animation::playback_parameters::SettableAttribute;
using s3::animation::skeleton_input::name::encode;

rs::Result<Settings, std::string> Settings::load(const s3::data::Collections& data) {
    S3_MOTION_TRY(const auto words, motion_collections::words<20>(data, "anim_motion", "hippy_flip",
                                                                   "antic_length_to_hippy_height"));
    return Settings{motion_collections::graph<8>(words, 4, 12)};
}

void State::update(MotionAnimation& animation, const Settings& settings) {
    assert(active);  // debug_assert!
    elapsed += 1.0f / 60.0f;
    animation.set_attribute(
        SettableAttribute{encode("TrickHeight"), settings.antic_length_to_hippy_height.evaluate(elapsed), false, -1});
}

}  // namespace s3::game::graph_host::motion_hippy_jump
