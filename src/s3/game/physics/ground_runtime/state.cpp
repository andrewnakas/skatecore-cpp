// Port of skate-game/src/physics/ground_runtime/state.rs.
#include "s3/game/physics/ground_runtime/state.hpp"

#include "s3/game/difficulty.hpp"
#include "s3/game/physics/animation_input.hpp"

namespace s3::game::physics::ground_runtime::state {

rs::Result<GroundState, std::string> GroundState::load(const sd::Collections& data, std::string_view /*mode*/,
                                                       bool human_player) {
    auto push = [&](const char* field) { return data.float_("physics_push", "default", field); };
    GroundState out{
        sc::riding::grounded::state::PhysicsGroundState::before_first_enter(human_player),
        sc::riding::pumping::state::PumpingState::reset_state(),
        sc::riding::speed_wobble::SpeedWobbleState{},
        sc::physics::manual::state::ManualState{},
        //Initial truck state; later board resets preserve activation timers.
        sc::riding::steering::TruckSteeringState{},
        sc::riding::speed_model::SpeedModelState{},
        0.0f,
        {},
        {},
        {},
        false,
        {},
    };
    out.wobble._0 = {};
    out.manual.filtered_angle_error = 0.0f;
    out.manual.target_angle = 0.0f;
    out.manual.measured_angle = 0.0f;
    out.manual.angular_correction = 0.0f;
    out.manual.elapsed = 0.0f;
    out.speed.target_speed = 0.0f;
    out.speed.flags_1360 = 0;
    S3G_TRY(out.pumping_settings, pumping::GroundPumping::load(data));
    S3G_TRY(out.output_settings.pushable_speed_terms_4_8[0], push("MaxPushableSpeed_CameraDelta"));
    S3G_TRY(out.output_settings.pushable_speed_terms_4_8[1], push("MaxPushableSpeed"));
    S3G_TRY(out.output_settings.mode_speed_threshold_0, push("Hash_501D5581043D7D3C"));
    S3G_TRY(out.entry_settings, entry::EntrySettings::load(data));
    for (std::size_t i = 0; i < difficulty::NATIVE_MODES.size(); ++i) {
        S3G_TRY(out.auto_push_enabled[i], data.boolean("physics_mode", difficulty::NATIVE_MODES[i], "AutoPushEnabled"));
    }
    out.entered = false;
    return out;
}

PhysicsGroundOutput GroundState::output(const sc::player::input_phase::ProcessedPhysicsInput& p,
                                        const animation_input::AnimationInput& animation,
                                        const sc::physics::board_toolkit::BoardToolkit& t) const {
    sc::riding::grounded::state::output::GroundOutputFrame frame;
    frame.axis_464 = rs::from_bits(p.vectors_464_480_496_512_528[0]);
    frame.velocity_608 = rs::from_bits(p.vectors_544_560_592_608[3]);
    frame.absolute_body_speed_2616 = t.absolute_speed;
    frame.deck_speed_2652 = p.scalar_2652;
    frame.state_timer_2664 = p.state_timer_2664;
    frame.scalar_2720 = animation.fields.balance;
    frame.flags_2476 = p.flags_2476;
    frame.flags_2484 = p.flags_2484;
    S3_ASSERT(p.state_variant_index_2528 < auto_push_enabled.size());
    frame.selected_mode_flag_109 = auto_push_enabled[static_cast<std::size_t>(p.state_variant_index_2528)];
    return sc::riding::grounded::state::output::fill_physics_output(this->state, frame, output_settings);
}

services::GroundControllers GroundState::controllers() {
    return services::GroundControllers{wobble, steering, speed, manual, heading_previous};
}

}  // namespace s3::game::physics::ground_runtime::state
