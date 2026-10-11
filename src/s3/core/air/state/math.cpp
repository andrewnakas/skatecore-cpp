// Port of skate-core/src/air/state/math.rs.
#include "s3/core/air/state/math.hpp"

#include "s3/core/math.hpp"
#include "s3/core/physics/board_ground.hpp"
#include "s3/core/physics/board_motion_output.hpp"
#include "s3/core/physics/native_arithmetic.hpp"

namespace s3::air::state::math {

namespace native_arithmetic = physics::native_arithmetic;
using physics::board_motion_output::inverse_length_squared;

float AirMath::minimum_vminfp(float left, float right) { return native_arithmetic::vector_min(left, right); }

float AirMath::length_squared_vmsum3fp(std::array<float, 4> value) { return native_arithmetic::dot3(value, value); }

float AirMath::length_vmsum3fp_vrsqrte(std::array<float, 4> value) {
    const float square = native_arithmetic::dot3(value, value);
    const float length = square * inverse_length_squared(square, 2);
    return square == 0.0f ? 0.0f : length;
}

std::array<float, 4> AirMath::clamp_vector_within_max_length(std::array<float, 4> value, float maximum) {
    // 82BD3D90..3E74, including the original short-vector bge and fsel.
    const float length = length_vmsum3fp_vrsqrte(value);
    if (!(length >= rs::from_bits(0x37800000u))) {
        return value;
    }
    const float bound = maximum - length >= -0.0f ? length : maximum;
    float inverse = native_arithmetic::reciprocal_estimate(length);
    for (int i = 0; i < 2; ++i) {
        inverse = rs::mul_add(inverse, rs::mul_add(-inverse, length, 1.0f), inverse);
    }
    std::array<float, 4> out{};
    for (std::size_t i = 0; i < 4; ++i) out[i] = (value[i] * bound) * inverse;
    return out;
}

float angle_between_vectors(std::array<float, 4> left, std::array<float, 4> right) {
    auto xyz = [](std::array<float, 4> v) { return s3::math::Vector3::new_(v[0], v[1], v[2]); };
    return physics::board_ground::angle_between(xyz(left), xyz(right));
}

std::array<float, 4> clamp_jump_velocity(std::array<float, 4> reference, std::array<float, 4> current) {
    const float reference_speed = AirMath{}.length_vmsum3fp_vrsqrte(reference);
    const float speed = AirMath{}.length_vmsum3fp_vrsqrte(current);
    const float denominator = speed - 1.0f >= -0.0f ? speed : 1.0f;
    const float scale = denominator > reference_speed ? reference_speed / denominator : 1.0f;
    return {current[0] * scale, current[1] * scale, current[2] * scale, current[3] * scale};
}

}  // namespace s3::air::state::math
