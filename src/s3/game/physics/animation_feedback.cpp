// Port of skate-game/src/physics/animation_feedback.rs.
#include "s3/game/physics/animation_feedback.hpp"

#include "s3/game/physics/animation_feedback_settings.hpp"

namespace s3::game::physics::animation_feedback {

rs::Result<AnimationFeedback, std::string> AnimationFeedback::load(const sd::Collections& data) {
    AnimationFeedback out;
    //82DEFAA0..FAC8 zeros the retained vector96.
    out.previous_lateral_tilt = {0.0f, 0.0f, 0.0f, 0.0f};
    out.published_previous_lateral_tilt = {0.0f, 0.0f, 0.0f, 0.0f};
    S3G_TRY(out.settings, animation_feedback_settings::load(data));
    // Global344 at8289FCA0 binds hash7823ADA8E7C896AA = bumps.
    S3G_TRY(out.bump_settings.scale_x_acc, data.float_("anim_motion", "bumps", "scale_x_acc"));
    S3G_TRY(out.bump_settings.min_bump_mag, data.float_("anim_motion", "bumps", "min_bump_mag"));
    //82DEFAA0 resets histories to zero. Every82DEFF38 update refreshes
    //the four coefficient words before using each filter.
    out.state.history = {};
    out.state.filters = {};
    return out;
}

void AnimationFeedback::reset() {
    state.reset_history();
    previous_lateral_tilt = {0.0f, 0.0f, 0.0f, 0.0f};
    published_previous_lateral_tilt = {0.0f, 0.0f, 0.0f, 0.0f};
}

sc::animation::physical_feedback::PhysicalFeedback AnimationFeedback::update(
    const sc::physics::board_motion_output::BoardMotionOutput& motion,
    const sc::riding::pumping::state::PumpingState& pumping,
    const sc::riding::speed_wobble::SpeedWobbleState& wobble,
    sc::animation::physical_feedback::ReckoningFeedback reckoning,
    sc::animation::physical_feedback::ControlFeedback controls,
    sc::animation::ground_acceleration::Input acceleration, std::array<float, 4> lateral_tilt) {
    //82DEFB6C..FB7C publishes the preceding Animation0 at Animation16,
    //then retains current Reckoning1248 for the next conditioner update.
    published_previous_lateral_tilt = previous_lateral_tilt;
    previous_lateral_tilt = lateral_tilt;
    const auto bump = sc::animation::ground_acceleration::publish(acceleration, bump_settings);
    return sc::animation::physical_feedback::publish(state, settings, motion, pumping, wobble, reckoning, controls,
                                                     bump);
}

}  // namespace s3::game::physics::animation_feedback
