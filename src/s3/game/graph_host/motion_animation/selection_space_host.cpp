// Port of skate-game/src/graph_host/motion_animation/selection_space_host.rs.
#include "s3/game/graph_host/motion_animation/selection_space_host.hpp"

namespace s3::game::graph_host::motion_animation {

rs::Result<rs::Unit, std::string> MotionAnimation::prepare_selection_spaces(
    PlaybackTree& tree, Slice<const SettableAttribute> attributes) {
    if (auto* space = std::get_if<PlaybackTree::SelectionSpace>(&tree.value)) {
        auto selected = space->_0.select(attributes);
        if (selected.is_err()) return selected;
        //82D26F88 calls IAnimatable+76, not SkaterAnim::GetAnimTree.
        //TU3 vtable8231E050+76 ->82E32328 ->82D19648 constructs a
        //raw child. The enclosing main/channel tree already owns its
        //bind pose and mirroring; applying those here doubles bone
        //translations and rotations (visible on offboard jump clips).
        PlaybackTree* current = space->_0.current_mut();
        if (!current) S3_PANIC("called `Option::unwrap()` on a `None` value");
        return prepare_selection_spaces(*current, attributes);
    }
    if (std::holds_alternative<PlaybackTree::BlendSpace>(tree.value)) {
        // Type6 SetAttributes82D24078 consumes its own parameters only.
        return rs::Unit{};
    }
    if (auto* space = std::get_if<PlaybackTree::PhaseBlend>(&tree.value)) {
        for (PlaybackTree& child : space->_0.children) {
            auto r = prepare_selection_spaces(child, attributes);
            if (r.is_err()) return r;
        }
        return rs::Unit{};
    }
    if (auto* transition = std::get_if<PlaybackTree::Transition>(&tree.value)) {
        // PlaybackTransition::set_attributes follows this same to/from order.
        auto r = prepare_selection_spaces(*transition->_0.to, attributes);
        if (r.is_err()) return r;
        if (transition->_0.settings.kind == 4 && !transition->_0.complete()) {
            return prepare_selection_spaces(*transition->_0.from, attributes);
        }
        return rs::Unit{};
    }
    if (auto* bind = std::get_if<PlaybackTree::BindPose>(&tree.value)) {
        return prepare_selection_spaces(*bind->motion, attributes);
    }
    return rs::Unit{};  // PlaybackTree::Clip
}

}  // namespace s3::game::graph_host::motion_animation
