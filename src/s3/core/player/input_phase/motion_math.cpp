// Port of skate-core/src/player/input_phase/motion_math.rs (whole file).
#include "s3/core/player/input_phase/motion_math.hpp"

#include "s3/core/physics/board_motion_output.hpp"
#include "s3/core/physics/native_arithmetic.hpp"

namespace s3::player::input_phase::motion_math {

using physics::board_motion_output::inverse_length_squared;
using physics::native_arithmetic::dot3;
using physics::native_arithmetic::reciprocal_estimate;

GroundHistoryResult ground_history(GroundHistoryRequest request) {
    const auto old = rs::from_bits(request.previous_position);
    const auto current = rs::from_bits(request.current_position);
    const auto previous = rs::from_bits(request.previous_filtered_delta);
    const auto delta = rs::from_fn<4>([&](std::size_t i) { return current[i] - old[i]; });
    const float timestep = rs::from_bits(request.timestep_bits);
    float reciprocal = reciprocal_estimate(timestep);
    for (int i = 0; i < 2; ++i) {
        const float error = rs::mul_add(-reciprocal, timestep, 1.0f);
        reciprocal = rs::mul_add(reciprocal, error, reciprocal);
    }
    // Literal82072818, then8231A844 minus that coefficient.
    const float retention = rs::from_bits(0x3f733333u);
    const float change = 1.0f - retention;
    GroundHistoryResult result;
    result.delta = rs::to_bits(delta);
    result.filtered_delta = rs::from_fn<4>([&](std::size_t i) {
        const float scaled = reciprocal * (delta[i] * change);
        return rs::to_bits(rs::mul_add(previous[i], retention, scaled));
    });
    return result;
}

RawVector prepared_jump_velocity(PrepareJumpRequest request) {
    const auto velocity = rs::from_bits(request.skateboard_vector);
    const auto previous = rs::from_bits(request.previous_velocity);
    const float squared = dot3(velocity, velocity);
    const float inverse = inverse_length_squared(squared, 2);
    const float length = squared == 0.0f ? 0.0f : squared * inverse;
    // Initializer82F826F8 broadcasts82181A88 into830BD350.
    std::array<float, 4> direction{};
    if (length > rs::from_bits(0x358637bdu)) {
        direction = rs::map(velocity, [&](float v) { return v * inverse; });
    }
    const float down_component = rs::from_bits(rs::to_bits(direction[1]) ^ 0x80000000u);
    const float acceleration = down_component * rs::from_bits(0x411ccccdu);  // 822F8BD4
    return rs::from_fn<4>([&](std::size_t i) {
        return rs::to_bits(rs::mul_add(direction[i] * acceleration, rs::from_bits(0x3c888889u),  // 820849C8
                                       previous[i]));
    });
}

}  // namespace s3::player::input_phase::motion_math
