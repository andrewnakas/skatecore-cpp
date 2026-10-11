// Port of skate-core/src/physics/manual/angle.rs.
#include "s3/core/physics/manual/angle.hpp"

#include <cstdint>
#include <limits>

namespace s3::physics::manual::angle {

rs::Result<float, AngleError> normalize(float angle) {
    const float pi = rs::from_bits(0x40490fdbu);
    if (angle >= -pi && angle < pi) {
        return angle;
    }
    const float turns = angle * rs::from_bits(0x3e22f983u);
    if (!rs::is_finite(turns) ||
        turns < static_cast<float>(std::numeric_limits<std::int32_t>::min()) ||
        static_cast<double>(turns) > static_cast<double>(std::numeric_limits<std::int32_t>::max())) {
        return rs::make_err(AngleError::IntegerConversionUnavailable);
    }
    const std::int32_t whole_turns = rs::as_i32(turns);
    const float tau = rs::from_bits(0x40c90fdbu);
    // fnmsubs performs fused -(count*tau - angle), then binary32 rounding.
    const float reduced = -rs::mul_add(static_cast<float>(whole_turns), tau, -angle);
    if (reduced >= pi) {
        return reduced - tau;
    }
    if (reduced < -pi) {
        return reduced + tau;
    }
    return reduced;
}

}  // namespace s3::physics::manual::angle
