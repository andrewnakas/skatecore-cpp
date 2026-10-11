// Port of skate-core/src/physics/centre_of_mass_filter.rs.
#include "s3/core/physics/centre_of_mass_filter.hpp"

#include "s3/core/physics/native_arithmetic.hpp"

namespace s3::physics::centre_of_mass_filter {

using native_arithmetic::dot3;
using native_arithmetic::reciprocal_estimate;

CentreOfMassOutput CentreOfMassFilter::update(std::array<float, 4> position,
                                              std::array<float, 4> velocity) {
    const float dt = rs::from_bits(0x3c888889u);
    const auto old_velocity = velocity_;
    velocity_ = rs::from_fn<4>(
        [&](std::size_t i) { return rs::mul_add(old_velocity[i], 0.75f, velocity[i] * 0.25f); });
    float inverse_dt = reciprocal_estimate(dt);
    inverse_dt = rs::mul_add(inverse_dt, rs::mul_add(-dt, inverse_dt, 1.0f), inverse_dt);
    inverse_dt = rs::mul_add(inverse_dt, rs::mul_add(-dt, inverse_dt, 1.0f), inverse_dt);
    const auto acceleration =
        rs::from_fn<4>([&](std::size_t i) { return (velocity_[i] - old_velocity[i]) * inverse_dt; });
    if (!position_valid_) {
        position_ = position;
        position_velocity_ = {0.0f, 0.0f, 0.0f, 0.0f};
        position_valid_ = true;
    } else {
        const std::array<float, 4> coefficient{0.05f, 0.03f, 0.05f, 0.0f};
        const std::array<float, 4> unity{1.0f, 1.0f, 1.0f, 0.0f};
        position_velocity_ = rs::from_fn<4>([&](std::size_t i) {
            return rs::mul_add(unity[i] - coefficient[i], position_velocity_[i],
                               coefficient[i] * velocity[i]);
        });
        const auto predicted = rs::from_fn<4>(
            [&](std::size_t i) { return rs::mul_add(position_velocity_[i], dt, position_[i]); });
        position_ = rs::from_fn<4>([&](std::size_t i) {
            return rs::mul_add(unity[i] - coefficient[i], predicted[i], coefficient[i] * position[i]);
        });
        const auto error = rs::from_fn<4>([&](std::size_t i) { return position[i] - position_[i]; });
        if (dot3(error, accumulated_error_) > 0.0f) {
            position_ = rs::from_fn<4>(
                [&](std::size_t i) { return rs::mul_add(accumulated_error_[i], 0.08f, position_[i]); });
        }
        accumulated_error_ = rs::from_fn<4>([&](std::size_t i) {
            return rs::mul_add(accumulated_error_[i], 0.95f, (position[i] - position_[i]) * 0.05f);
        });
    }
    return CentreOfMassOutput{velocity_, acceleration, position_};
}

}  // namespace s3::physics::centre_of_mass_filter
