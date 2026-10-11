// Port of skate-core/src/riding/grounded/propulsion.rs.
#include "s3/core/riding/grounded/propulsion.hpp"

namespace s3::riding::grounded::propulsion {

GroundPropulsion calculate(GroundPropulsionInput input, GroundPropulsionSettings settings,
                           std::uint8_t& push_suppressed_2730) {
    braking::BrakeInput brake;
    brake.flags_2468 = input.flags_2468;
    brake.input_2728 = input.brake_input;
    brake.signed_speed = input.signed_speed;
    brake.absolute_body_speed = input.absolute_body_speed;
    brake.surface_factor = input.surface_braking_factor;
    brake.direction = input.brake_direction;
    const QueuedPointForce braking_force = braking::calculate_braking(brake, settings.braking);
    push::PushInput push_input;
    push_input.flags_2468 = input.flags_2468;
    push_input.flags_2472 = input.flags_2472;
    push_input.target_speed = input.target_speed;
    push_input.current_speed = input.signed_speed;
    push_input.absolute_body_speed = input.absolute_body_speed;
    push_input.scale = input.scalar_2660;
    push_input.delta_seconds = input.timestep;
    push_input.direction = input.push_direction;
    push::PushLimits limits;
    limits.maximum_pushable_speed = settings.maximum_pushable_speed;
    limits.low_speed_change = settings.mode_speed_changes[0];
    limits.high_speed_change = settings.mode_speed_changes[1];
    const PushAcceleration push_result = push::calculate_acceleration(push_input, limits);
    push_suppressed_2730 = push_result.suppressed ? 1 : 0;
    return GroundPropulsion{braking_force, push_result};
}

PropulsionSubmission GroundPropulsion::submit(const ManualEffect& manual, BoardForceQueue& queue) const {
    if (manual.correction_active) {
        const auto& force = manual.corrective_force_world;
        const auto& point = manual.corrective_point_body;
        return PropulsionSubmission::ManualCorrection{queue.append(QueuedPointForce{
            7, Vector3{force[0], force[1], force[2]}, Vector3{point[0], point[1], point[2]}})};
    }
    const bool brake = queue.append(braking);
    const bool pushed = queue.append(QueuedPointForce{3, push.vector, push.local_point});
    return PropulsionSubmission::BrakingAndPush{{brake, pushed}};
}

}  // namespace s3::riding::grounded::propulsion
