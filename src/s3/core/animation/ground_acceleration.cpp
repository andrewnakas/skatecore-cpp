// Port of skate-core/src/animation/ground_acceleration.rs.
#include "s3/core/animation/ground_acceleration.hpp"

#include "s3/core/physics/native_arithmetic.hpp"

namespace s3::animation::ground_acceleration {

namespace na = physics::native_arithmetic;

Output publish(Input input, const Settings& settings) {
    const std::array<float, 4> acceleration = condition(input);
    return Output{acceleration, is_bumped(acceleration, settings)};
}

std::array<float, 4> condition(Input input) {
    std::array<float, 4> deck_local = inverse_rotate(input.deck, input.world_acceleration);
    deck_local[1] = 0.0f;  //82DF05C4
    const std::array<float, 4> world = rotate(input.deck, deck_local);
    std::array<float, 4> ground_local = inverse_rotate(input.ground, world);
    ground_local[1] = 0.0f;  //82DF0600
    return ground_local;
}

bool is_bumped(std::array<float, 4> acceleration, const Settings& settings) {
    acceleration[0] *= settings.scale_x_acc;
    const float squared = na::dot3(acceleration, acceleration);
    float reciprocal = na::reciprocal_square_root_estimate(squared);
    for (int i = 0; i < 2; ++i) {
        const float square = reciprocal * reciprocal;
        const float half = reciprocal * 0.5f;
        const float error = rs::mul_add(-squared, square, 1.0f);
        reciprocal = rs::mul_add(half, error, reciprocal);
    }
    const float magnitude = squared == 0.0f ? 0.0f : squared * reciprocal;
    return magnitude > settings.min_bump_mag;
}

std::array<float, 4> inverse_rotate(const AnimationPartTransform& frame, std::array<float, 4> vector) {
    return rs::from_fn<4>([&](std::size_t lane) {
        return rs::mul_add(frame[lane][2], vector[2], rs::mul_add(frame[lane][1], vector[1], frame[lane][0] * vector[0]));
    });
}

std::array<float, 4> rotate(const AnimationPartTransform& frame, std::array<float, 4> vector) {
    return rs::from_fn<4>([&](std::size_t lane) {
        return rs::mul_add(frame[2][lane], vector[2], rs::mul_add(frame[1][lane], vector[1], frame[0][lane] * vector[0]));
    });
}

}  // namespace s3::animation::ground_acceleration
