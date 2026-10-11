// Port of skate-game/src/physics/ground_runtime/update.rs.
#include "s3/game/physics/ground_runtime/update.hpp"

#include "s3/core/riding/grounded/state/motion.hpp"
#include "s3/game/physics/biped_ground/grab_runtime.hpp"
#include "s3/game/physics/foot_ik.hpp"
#include "s3/game/physics/ground_runtime.hpp"

namespace s3::game::physics::ground_runtime::state {

rs::Result<sc::riding::grounded::state::board::GroundBoardOutcome, std::string> GroundState::update(
    GroundRuntime& runtime, sc::physics::board_runtime::BoardRuntime& board,
    const sc::physics::board_world::BoardWorld& world, const settings::GroundSettings& settings,
    update::GroundUpdateFrame frame, services::GroundPhysicalFrame physical, update::GroundUpdateTargets targets) {
    if (!entered) {
        return rs::make_err(std::string("Ground::Enter must finish before Ground::Update"));
    }
    const auto& p = frame.processed;
    const auto signals = this->state.begin_update_at(static_cast<std::int32_t>(p.wheel_count_2556),
                                                     frame.toolkit.deck[3]);
    if (signals.set_skeleton_flag_16505) {
        targets.skeleton_elapsed_16505 = true;
    }
    //GrabWorld (2476 bit22) requests Handplant::ground_query82D38430,
    //already submitted by ground_phase. It is not proof of a Skitch object.
    //The same Handplant owner handles the inactive reset and a query miss;
    //ordinary ground forces continue for both held and released input.
    S3G_TRY(const auto pumping_mode, pumping_settings.mode(p.state_variant_index_2528));
    pumping_settings.update(pumping, frame.toolkit, frame.riding, frame.skeleton, p.flags_2476, pumping_mode);
    auto input = settings.input(frame.toolkit, p, frame.animation, pumping, pumping_mode.unintentional_scalar,
                                frame.riding, frame.skeleton, frame.base_trucks, frame.extra);
    services::GroundControllers controllers{wobble, steering, speed, manual, heading_previous};
    S3G_TRY(auto outcome, runtime.update_board(board, world, settings, this->state, controllers, input,
                                               std::move(physical)));
    const auto normal = rs::from_bits(p.vectors_464_480_496_512_528[0]);
    if (auto delta = sc::riding::grounded::state::motion::future_deck_displacement(
            board.forces(), frame.toolkit.total_mass, p.timestep_2604, normal, this->state.flag_2720,
            this->state.manual_correction_2732)) {
        S3G_CHECK(targets.move_future_deck(*delta));
    }
    if (this->state.flag_2722) {
        targets.foot_ik.state.contacts.support_failed_this_update = true;
    }
    targets.board_correction_pending = true;
    this->state.finish_update(p.timestep_2604);
    //82D37F10 calls82D749D0 on the same PlayerGrabSpline owner as offboard.
    targets.offboard_grab.invalidate();
    return outcome;
}

}  // namespace s3::game::physics::ground_runtime::state
