// Port of skate-game/src/graph_host/motion_toggle_execute.rs (module `motion::toggle_execution`).
#include "s3/game/graph_host/motion/toggle_execution.hpp"

#include "s3/game/graph_host/motion/execution.hpp"

namespace s3::game::graph_host::motion {

rs::Result<rs::Unit, std::string> MotionHost::execute_toggle_board(BehaviorId behavior, std::uint8_t phase) {
    if (behavior >= instances.size()) return rs::make_err(std::string("Unallocated ToggleBoard behavior"));
    auto* state = instances[behavior].get<Instance::ToggleBoard>();
    if (state == nullptr) return rs::make_err(std::string("ToggleBoard operation/instance mismatch"));
    const std::optional<bool> mirrored =
        animation.skater_animation_flags
            ? std::optional<bool>((*animation.skater_animation_flags & 0x40000000u) != 0)
            : std::nullopt;
    //82BA9060/9080 reads Actor+20 virtual16, not raw AG input.
    //8259176C binds Actor+20 to IMotionGraph (8258F180); vtable
    //823006D0+16 ->8258F330 returns MG+12, its authored intention map.
    //The stock OffBoard AG state controls when these requests exist.
    const IntentMap& intents = animation.motion_intents;
    outputs::BoardControls board{};
    board.drop_requested = intents.contains_key("OB_DropBoard");
    board.throw_requested = intents.contains_key("OB_ThrowBoard");
    board.retrieve_requested = intents.contains_key("OB_RetrieveBoard");
    return motion_toggle_board::execute(state->_0, animation, board, toggle_board_physical, mirrored, phase);
}

}  // namespace s3::game::graph_host::motion
