// Port of skate-game/src/graph_host/motion_hooks.rs (out-of-line items).
#include "s3/game/graph_host/motion_hooks.hpp"

#include "s3/game/graph_host/motion_nodes.hpp"

namespace s3::game::graph_host::motion_hooks {

std::optional<MotionHook> MotionHook::parse(const Attributes& a) {
    const std::string* name = a.text("name");
    if (!name) return std::nullopt;
    if (*name == "GrabSlide") return MotionHook(GrabSlide{a.boolean_byte("right", 1) != 0});
    if (*name == "OverideNextAnimTransitionHook") return MotionHook(Override{motion_nodes::transition(a)});
    if (*name == "MongoPushToAntic") {
        const std::string* animation = a.text("anim");
        return MotionHook(MongoPushToAntic{animation ? *animation : std::string()});
    }
    return std::nullopt;
}

}  // namespace s3::game::graph_host::motion_hooks
