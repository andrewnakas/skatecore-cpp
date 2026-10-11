// Port of skate-core/src/riding/grounded/state/output.rs.
#include "s3/core/riding/grounded/state/output.hpp"

#include "s3/core/physics/native_arithmetic.hpp"

namespace s3::riding::grounded::state::output {

namespace {

float flush_subnormal(float value) {
    const std::uint32_t bits = rs::to_bits(value);
    if ((bits & 0x7f800000u) == 0 && (bits & 0x007fffffu) != 0) {
        return rs::from_bits(bits & 0x80000000u);
    }
    return value;
}

Vector4 reject_axis_component(Vector4 velocity, Vector4 axis) {
    // Direct TU3 sequence: vmsum3fp128, vmulfp128, vsubfp128. `dot3` is the
    // isolated, explicitly documented host approximation for the Xenon sum.
    const float projection = physics::native_arithmetic::dot3(velocity, axis);
    Vector4 out{};
    for (std::size_t lane = 0; lane < 4; ++lane) {
        const float scaled = flush_subnormal(axis[lane] * projection);
        out[lane] = flush_subnormal(velocity[lane] - scaled);
    }
    return out;
}

}  // namespace

PhysicsGroundOutput fill_physics_output(const PhysicsGroundState& state, GroundOutputFrame frame,
                                        GroundOutputSettings settings) {
    const bool has_world_grab_intent = (frame.flags_2476 & 0x00400000u) != 0;
    std::optional<GroundVelocityProjectionOutput> velocity_projection_36;
    if (frame.state_timer_2664 == 0.0f) {
        velocity_projection_36 =
            GroundVelocityProjectionOutput{reject_axis_component(frame.velocity_608, frame.axis_464), true};
    }

    PhysicsGroundOutput out;
    out.skateboard_motion_4 = SkateboardMotionOutput{
        state.flag_2721,
        frame.absolute_body_speed_2616 < settings.pushable_speed_terms_4_8[0] + settings.pushable_speed_terms_4_8[1],
    };
    out.velocity_projection_36 = velocity_projection_36;
    out.ground_32.wall_ride_exit = state.flag_2720;
    out.ground_32.anti_flip_nudge_present = state.anti_flip_nudge_applied_2723;
    out.ground_32.is_pinning = state.pinning_2727;
    out.ground_32.anti_flip_torque = state.anti_flip_torque_2624;
    out.ground_32.time_to_skitch = state.scalar_2664;
    out.ground_32.skitch_spline_height = state.scalar_2668;
    out.ground_32.processed_scalar_2720 = frame.scalar_2720;
    out.ground_32.processed_flag_2484_bit_13 = (frame.flags_2484 & 0x00002000u) != 0;
    out.state_28.grab_spline_type = state.word_2560;
    out.state_28.grab_spline_object_id = state.word_2564;
    out.state_28.flag_84 = state.flag_2731;
    out.state_28.has_world_grab_intent_without_object = has_world_grab_intent && !state.flag_2729;
    out.state_28.manual_correction_write_78 =
        state.manual_correction_2732 ? std::optional<bool>(true) : std::nullopt;
    out.intents_52.has_world_grab_intent = has_world_grab_intent;
    out.intents_52.selected_mode_below_speed_threshold_58 =
        frame.selected_mode_flag_109 && frame.deck_speed_2652 < settings.mode_speed_threshold_0;
    out.is_grabbing_object_72_304 = state.flag_2729;
    out.manual_opposition_56_168 = state.manual_opposition_2733;
    out.push_suppressed_20_596 = state.push_suppressed_2730;
    return out;
}

}  // namespace s3::riding::grounded::state::output
