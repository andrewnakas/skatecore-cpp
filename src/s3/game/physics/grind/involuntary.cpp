// Port of skate-game/src/physics/grind/involuntary.rs.
#include "s3/game/physics/grind/involuntary.hpp"

#include "s3/core/physics/grind_forces/release.hpp"
#include "s3/game/physics.hpp"
#include "s3/game/physics/grind.hpp"
#include "s3/game/physics/skater.hpp"

namespace s3::game::physics::grind::involuntary {

using arithmetic::dot3;
using ::s3::physics::grind_forces::release::Input;

Status advance(GamePhysics& physics, SkaterRuntime& skater, const ManagerObservation& manager) {
    S3G_TRY_OPT(const Family family, skater.grind.active, "Grind exit requires active family");
    if (!skater.player_input.toolkit) return rs::make_err(std::string("Grind exit requires BoardToolkit"));
    const auto frame = skater.player_input.toolkit->deck;
    auto& state = skater.grind.states[index(family)];
    const std::uint32_t kind = manager.geometry.kind_1464;
    Input input;
    input.position = frame[3];
    input.point = manager.geometry.point_1120;
    input.across = state.output.across;
    input.normal = state.output.normal;
    input.velocity = rs::from_bits(skater.player_input.processed.vectors_400_416[0]);
    input.geometry_kind = kind;
    input.high_side = manager.geometry.high_side_1440;
    input.force_across = false;
    input.enable_lift = true;
    input.strength = 100.0f;
    input.speed_limit = 1.0f;
    input.lift = 125.0f;
    switch (family) {
        case Family::Boardslide:
        case Family::Darkslide:
            //82D41250: rail branches request10 and publish direction*136 to
            //Wipeout16; only the ledge branch applies the lateral exit force.
            if (kind == 2) {
                input.strength = 90.0f;
                input.speed_limit = 0.75f;
                input.enable_lift = false;
                forces::release(physics.board, input);
            } else {
                state.output.slide_wipeout = true;
                for (std::size_t i = 0; i < 4; ++i) {
                    state.output.slide_impulse[i] = manager.geometry.direction_1136[i] * 136.0f;
                }
                skater.wipeout.state.request(10, 0.0f);
            }
            lifecycle::clear_wheel_spin(physics.board);
            break;
        case Family::FiftyFifty:
            if (kind >= 2) {
                input.strength = 10.0f;
                input.lift = 0.0f;
            }
            forces::release(physics.board, input);
            break;
        case Family::FiveO:
            input.strength = kind < 2 ? 120.0f : 10.0f;
            input.lift = kind < 2 ? 125.0f : 0.0f;
            input.speed_limit = 0.9f;
            // Raw82D42948/4C passes computed force-across in r8 and lift in r7.
            input.force_across = kind == 1 && rs::abs(dot3(frame[2], input.across)) > 0.23f;
            forces::release(physics.board, input);
            break;
        case Family::Backslash:
            input.speed_limit = 0.75f;
            input.force_across = kind == 1;
            input.enable_lift = kind == 1;
            forces::release(physics.board, input);
            lifecycle::clear_wheel_spin(physics.board);
            break;
        case Family::Tipslide:
            lifecycle::clear_wheel_spin(physics.board);
            if ((manager.geometry.flags_1476 & 0x08000000u) != 0 || state.output.tipslide_97_98_99[0]) {
                if (!state.output.tipslide_97_98_99[0]) {
                    physics.board.hook_mut().drive.disable_animation(skater.ground_lifecycle.board_animated_290);
                    state.output.tipslide_97_98_99[0] = true;
                    state.output.tipslide_97_98_99[2] = true;
                }
                S3G_CHECK(tip_nudge::apply(physics.board, input.position, input.point, input.across));
            } else {
                physics.board.hook_mut().drive.enable_angular_only(skater.ground_lifecycle.board_animated_290);
                input.speed_limit = 1.5f;
                //82D420EC/F0: r8=1 (across), r7=0 (no normal lift).
                input.force_across = true;
                input.enable_lift = false;
                forces::release(physics.board, input);
                state.output.tipslide_97_98_99[1] = true;
                if ((manager.control.flags_2488 & 0x10000000u) != 0) {
                    state.output.tipslide_97_98_99[2] = true;
                }
            }
            break;
    }
    return ok();
}

}  // namespace s3::game::physics::grind::involuntary
