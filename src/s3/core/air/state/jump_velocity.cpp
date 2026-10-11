// Port of skate-core/src/air/state/jump_velocity.rs.
#include "s3/core/air/state/jump_velocity.hpp"

namespace s3::air::state::jump_velocity {

Vector4 calculate_velocity_from_jump(const PhysicsAirFrame& frame, std::int32_t frames, PhysicsAirMath& math) {
    const float frame_seconds = frame.delta_time_2604 * static_cast<float>(frames);
    const float gravity_delta = frame_seconds * frame.gravity_y_2648;
    Vector4 predicted = frame.jump_velocity_848;
    predicted[0] += 0.0f;
    predicted[1] += gravity_delta;
    predicted[2] += 0.0f;
    predicted[3] += 0.0f;

    const Vector4 current = frame.current_velocity_400;
    Vector4 error{};
    for (std::size_t lane = 0; lane < 4; ++lane) error[lane] = current[lane] - predicted[lane];
    Vector4 output = predicted;

    // `vcmpgtfp(error.y, -3.0)`: false, including equality/unordered, selects
    // the current vertical velocity.
    if (!(error[1] > VERTICAL_ERROR_FLOOR)) {
        output[1] = current[1];
    }

    error[1] = 0.0f;
    const float horizontal_error_squared = math.length_squared_vmsum3fp(error);
    // Native fcmpu+blt skips correction only for an ordered value below 9.0.
    if (!(horizontal_error_squared < HORIZONTAL_ERROR_LIMIT_SQUARED)) {
        Vector4 predicted_horizontal = output;
        predicted_horizontal[1] = 0.0f;
        const float maximum_length = math.length_vmsum3fp_vrsqrte(predicted_horizontal);

        Vector4 current_horizontal = current;
        current_horizontal[1] = 0.0f;
        const Vector4 clamped = math.clamp_vector_within_max_length(current_horizontal, maximum_length);
        output[0] = clamped[0];
        output[2] = clamped[2];
    }
    return output;
}

}  // namespace s3::air::state::jump_velocity
