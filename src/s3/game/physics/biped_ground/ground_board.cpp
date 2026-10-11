// Port of skate-game/src/physics/biped_ground/ground_board.rs
//! Ground carried-board Update82D31D1C..DB0. State60 is SkateboardController;
//! state52 in Sync82D324B0 is a DIFFERENT grab-spline manager.
#include "s3/game/physics/biped_ground/ground_board.hpp"

#include "s3/game/physics.hpp"
#include "s3/game/physics/offboard/board_manager.hpp"
#include "s3/game/physics/skater.hpp"

namespace s3::game::physics::biped_ground::ground_board {

/// Executes original Stop/Hold effects on the existing possession/solver owner.
/// Does not run its common per-frame Update a second time.
void update_possession(GamePhysics& physics, SkaterRuntime& skater) {
    const std::uint32_t flags = skater.player_input.processed.flags_2484;
    const auto state = skater.skateboard_controller.fields.state_448;
    std::uint32_t target = 0;
    if ((flags & 1u) != 0) {
        if (state == 2 || state == 3 || state == 4) {
            return;
        }
        target = 5;
    } else {
        if (state != 5) {
            return;
        }
        target = 1;
    }
    skater.skateboard_controller.fields.word_444 = 0;
    if (state == target) {
        return;
    }
    const auto observation = offboard::board_manager::runtime::observe(physics, skater);
    auto effects = skater.board_possession_live.effects(physics, skater.ground_lifecycle.board_animated_290,
                                                        skater.player_input.processed.timestep_2604);
    if (target == 5) {
        skater.board_possession.stop(skater.skateboard_controller.fields, observation, effects);
    } else {
        skater.board_possession.hold(skater.skateboard_controller.fields, observation, effects);
    }
    //82D31D80/DB0 assigns state only AFTER the physical action.
    skater.skateboard_controller.fields.state_448 = target;
    skater.board_possession_live.publish_volumes(physics);
}

}  // namespace s3::game::physics::biped_ground::ground_board
