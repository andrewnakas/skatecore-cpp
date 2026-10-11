// Port of skate-core/src/riding/pumping.rs.
#include "s3/core/riding/pumping.hpp"

#include "s3/core/riding/vector.hpp"

namespace s3::riding::pumping {

std::array<float, 8> calculate(const PumpForceInput& i) {
    const float pumping = (i.flags_2476 & 2u) == 0 ? i.mode_multiplier * i.pumping_scalar : i.pumping_scalar;
    const float scalar = (i.input_scalar_2660 * pumping) / i.timestep;
    const auto direction = vector::normalize(i.direction_432, i.normal_threshold);
    std::array<float, 8> result{};
    for (std::size_t n = 0; n < 4; ++n) result[n] = direction[n] * scalar;
    return result;
}

}  // namespace s3::riding::pumping
