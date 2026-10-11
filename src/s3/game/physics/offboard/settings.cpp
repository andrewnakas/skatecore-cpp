// Port of skate-game/src/physics/offboard/settings.rs
#include "s3/game/physics/offboard/settings.hpp"

namespace s3::game::physics::offboard::settings {

namespace ob = ::s3::player::offboard;

rs::Result<Settings, std::string> Settings::load(const sd::Collections& data, const sd::AnimationMetadata& metadata) {
    auto biped = [&](const char* name) { return curves::load<8>(data, "physics_biped", name); };
    S3G_TRY(auto sprint, biped("Hash_6B93C51256A30FB4"));
    const auto sprint_blend = sprint.first;
    const auto bounds = sprint.second;
    Settings out{};
    // Field initialisers in Rust struct-literal order.
    S3G_TRY(auto sprint_speed, curves::load<4>(data, "physics_biped", "Hash_7209DCFDF3015EBF"));
    out.controller.movement_intent.sprint_speed = sprint_speed.first;
    S3G_TRY(auto normal_speed, curves::load<16>(data, "physics_biped", "SpeedVsInput"));
    out.controller.movement_intent.normal_speed = normal_speed.first;
    out.controller.movement_intent.sprint_blend = sprint_blend;
    out.controller.movement_intent.sprint_time_cap = bounds[2];
    S3G_TRY(auto slide_steering, biped("AutoTurnVsAngle"));
    out.controller.movement_intent.slide_steering = slide_steering.first;
    S3G_TRY(auto slope_speed_scalar, biped("Hash_31309236050A8F09"));
    out.controller.movement_velocity.slope_speed_scalar = slope_speed_scalar.first;
    S3G_TRY(auto slope_mode_speed, biped("Hash_CE45C724B30F9134"));
    out.controller.movement_velocity.slope_mode_speed = slope_mode_speed.first;
    S3G_TRY(auto turn_vs_speed, biped("TurnVsSpeed"));
    out.controller.movement_velocity.turn_vs_speed = turn_vs_speed.first;
    S3G_TRY(auto turn_delta_vs_speed, biped("TurnDeltaVsSpeed"));
    out.controller.movement_velocity.turn_delta_vs_speed = turn_delta_vs_speed.first;
    S3G_TRY(auto slide_vs_slope, biped("SlideVsSlope"));
    out.controller.slide_vs_slope = slide_vs_slope.first;
    S3G_TRY(auto slide_vs_speed, biped("SlideVsSpeed"));
    out.controller.slide_vs_speed = slide_vs_speed.first;
    S3G_TRY(out.metrics, metrics::load(metadata));
    S3G_TRY(out.board, board::load(data));
    S3G_TRY(auto movement, curves::load<8>(data, "physics_state_offboard", "Hash_DF759B46440F16E9"));
    out.movement_vs_stick_angle = movement.first;
    S3G_TRY(auto turn, curves::load<8>(data, "physics_state_offboard", "TurnVsStickAngle"));
    out.turn_vs_stick_angle = turn.first;
    S3G_TRY(out.air_launch.jump_speed_scalar, data.float_("physics_biped", "default", "JumpSpeedScalar"));
    S3G_TRY(out.air_launch.jump_height, data.float_("physics_biped", "default", "JumpHeight"));
    return out;
}

std::tuple<ob::controller::Settings, std::array<std::optional<ob::controller::ClipMetric>, 3>, PointGraph<8>,
           PointGraph<8>, ob::air_launch::Settings>
Settings::into_controller_parts() && {
    return {std::move(controller), metrics, movement_vs_stick_angle, turn_vs_stick_angle, air_launch};
}

}  // namespace s3::game::physics::offboard::settings
