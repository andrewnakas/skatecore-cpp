// Port of skate-game/src/graph_host/motion_grind/animation_owner.rs.
#include "s3/game/graph_host/motion_animation/grind_owner.hpp"

namespace s3::game::graph_host::motion_animation {

rs::Result<bool, std::string> MotionAnimation::query_current_attribute(AttributeName name, std::uint32_t mask,
                                                                       AnimationAttribute& output) const {
    // S3 virtual48 ->825310E0 returns the current root at owner+13524.
    // Query that root, retaining transition routing and partial results.
    if (!current) return rs::make_err(std::string("GrindControlFade requires a current animation tree"));
    return current->query_attribute(name, mask, output);
}

}  // namespace s3::game::graph_host::motion_animation
